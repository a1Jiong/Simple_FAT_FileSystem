#define BLOCKSIZE 1024  //磁盘块大小
#define DISKSIZE 1024000    //虚拟磁盘空间大小

#define END 65535       //FAT中的文件结束标志
#define FREE 0          //FAT中盘块空闲标志
#define NOSPACE END     //表示没有多余位置

#define ROOTBLOCKNUM 2  //根目录初始所占盘块总数

#define MAXOPENFILE 10  //最多同时打开文件个数

#define ROOTSTARTBLOCK 0     //ROOT初始数据块编号

#define SUPERBLOCKNUM  1    //superblock占用的块数
#define FATBLOCKNUM 2       //FAT占用的块数
#define DATABLOCKNUM 995    //数据块占用的块数

struct FAT
{
    unsigned short id;
};

struct FCB 
{   //8 + 1 + 2 + 2 + 2 + 8 + 1 = 24B
    char filename[8];           //文件名
    unsigned char attribute;    //文件属性字段，0：目录文件；1：数据文件
    unsigned long time;        //文件创建时间
    unsigned short first;       //文件起始盘块号
    unsigned long length;       //文件长度（字节数）
    char free;                  //表示目录项是否为空，若值为 0，表示空，值为 1，表示已分配
    unsigned char padding[486];
};

struct USEROPEN
{
    char filename[8];           //文件名
    unsigned char attribute;    //文件属性：值为 0 表示目录文件，值为 1 表示数据文件
    unsigned long time;        //文件创建时间
    unsigned short first;       //文件起始盘块号
    unsigned long length;       //文件长度
    //上面内容是文件的 FCB 中的内容，下面是文件使用中的动态信息
    
    FCB *fcb;
    char dir [80];  //打开文件所在路径，以便快速检查指定文件是否已经打开
    int count;      //读写指针的位置
    char fcbstate;  //文件的 FCB 是否被修改，如果修改了置为 1，否则为 0
    char free; //打开表项是否为空，若值为 1，表示可用，否则表示已被占用
};

struct SUPERBLOCK
{
    char information[200];      //存储一些描述信息，如磁盘块大小、磁盘块数量等
    unsigned short fat_base;
    unsigned short fat2_base;
    unsigned short data_block_base;  //虚拟磁盘上数据区开始位置
    FCB root;        //根目录的文件控制块
};

unsigned char *vdisk; //指向虚拟磁盘的起始地址

SUPERBLOCK *superblock;

FAT *fat_base;

FAT *fat2_base;

unsigned char* data_block_base;  //记录虚拟磁盘上数据区开始位置

FCB *curfcb;             //当前目录fd的文件描述符 fd

char curdir[80];    //记录当前目录的目录名（包括目录的路径）

USEROPEN openfilelist[MAXOPENFILE]; //用户打开文件表数组