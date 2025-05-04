## Intruduction
part3中的log设计.主要修改包括：
- 在磁盘布局中加入log区域，superblock中补充相关的log元数据
- block manager需要修改，因为最终完成写入的是bm，其需要保存写入块的信息，包括<块号，块数据>
- commit log会类似inode manager和block allocator，有自己管理的那部分磁盘空间  

### disk layout
src/include/meta/manager.h记录了manager管理inode后的磁盘块分布.  
```txt
| Super block | Inode Table   | Inode allocation bitmap | Block allocation bitmap ... |  Other data blocks   |  
```
将log占用的磁盘块放置在super block中，且管理log需要的元数据信息存放在super block中.  
```txt
| Super block | Log region |Inode Table   | Inode allocation bitmap | Block allocation bitmap ... |  Other data blocks   |  
```
实验描述和代码中的一些全局变量不太符合，比如:  
- 8MB的文件、块大小为4KB，默认的总块数应该是2K，但是代码中却是4K.但是遵循代码的实现，默认文件大小为16MB，总块数为4K
- 且代码中支持最大的log大小为40MB（不知道是不是默认的，还是不管文件多大支持最大为40MB），而实验描述中log最大为1024块.  
块大小和数量是支持自定义构造block manager的。依据默认的4K个块1K个log空间，我们默认使用的log块数为总块数的1/4.   
### data structure  
Log region中，事务的修改的块以及需要的元数据依次写入磁盘，一般来说，事务写入和提交为header block + updated block* + commit block，为了节省空间和保持简单（这里似乎并不需要比较复杂的事务实现），将Log region分为两部分，一部分保存每个事务的一些元数据，包括事务号、更新的磁盘块等等，另一部分具体包括每个事务更新的数据块.可能是因为个人比较想让按块更新的数据块也以块对齐的方式存放在log区域。  
对于元数据，每个提交事务会按顺序写入它相关的log entry，包括：
- txn_id：事务id
- update_block_begin：存放更新块的区域的起始块id
- update_block_end：存放更新块的区域的下一个块id
- update_block_arr*：更新块的块号，有多个  
第二部分则是简单的，一系列块，存放每个事务的更新的块，顺序和位置由log entry中的信息跟踪。
这是Log region的划分：
```txt
| Log entry array | Trans updated blocks |  
```
这是log entry和事务更新的块区域的关系
```txt
/*
*               __________________________________________________________________
*              |                                                                  |
*              |                                                                  ↓
*  | id begin end arr| ... | txn(i) update blocks |   txn(i + 1) update blocks    |
*                                                 ↑
*           |_____________________________________|
*/  
由于log整体大小超过kMaxLogSize(128)就需要checkpoint，我们假设事务更新的数据块不超过128，每个提交事务将其LogEntry顺序写入到Log region的Log entry arr区域（为节省空间不做对齐要求）。  
一个更新了128个块的事务的log entry大小稍超过1KB，默认情况下的1/4块大小；一个更新了一个块的事务的log entry大小不超过32B，128个这样的事务的log entry占用磁盘空间大小不超过4KB。  
我们使用4KB区域，存放log entry。在log entry arr满或Trans updated blocks超过kMaxLogSize，执行checkpoint
```
### log write & commit