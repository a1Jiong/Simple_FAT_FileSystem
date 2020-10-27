#include "data_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define set_fat_value(no, value)    \
    (fat_base + (no))->id = (value);    \
    (fat2_base+ (no))->id = (value);

#define get_fat_entry_addr(no) \
    (fat_base + (no))

#define get_fat_entry_no(addr)  \
    ((FAT *)(addr) - fat_base)

#define get_fat_value(no)  \
    get_fat_entry_addr(no)->id

#define get_data_block_addr(no)  \
    (data_block_base + (no) * BLOCKSIZE)

#define get_data_block_no(addr) \
    (((addr) - data_block_base) / BLOCKSIZE)

#define next_data_block_no(no)  \
    get_fat_entry_addr(no)->id

#define set_fcb(fcb, attr, timevalue, name, firstvalue, len, freevalue)    \
    strncpy(fcb->filename, name, 8);    \
    fcb->first = firstvalue;  \
    fcb->attribute = attr;  \
    fcb->time = timevalue;\
    fcb->length = len;  \
    fcb->free = freevalue;

#define fcb_len_add(fcb, len)   \
    fcb->length += len;

#define fcb_for_each(pos, start_no) \
    for (; \
        start_no != END;    \
        moveto_next_fcb(pos, start_no))    \

#define useropen_for_each()  \
    for (int i = 0; i < MAXOPENFILE; i++)

#define initialization()    \
    superblock = (SUPERBLOCK *)vdisk;   \
    fat_base = (FAT *)(vdisk + superblock->fat_base * BLOCKSIZE);   \
    fat2_base = (FAT *)(vdisk + superblock->fat2_base * BLOCKSIZE); \
    data_block_base = (unsigned char *)(vdisk + superblock->data_block_base * BLOCKSIZE);   \
    curfcb = &(superblock->root);   \
    strncpy(curdir, "/", 80);   \
    useropen_for_each() \
        openfilelist[i].free = 1;

FCB *find_fcb_by_name(FCB* start_fcb, const char *name);
void moveto_next_fcb(FCB *&pos, unsigned short &start_no);
unsigned short find_free_block();
FCB *find_free_fcb(FCB *cur_fcb);
unsigned short append_file(FCB *file, int block_num);
unsigned short alloc_blocks(int len);
void format_blocks_to_fcbs(unsigned short start_no);
FCB *find_fcb_by_dir(const char *dir, char *new_dir);
FCB *parent_fcb(FCB *child);
void split_path(const char *path, char *dir, char *file);
void free_blocks(FCB *target);

unsigned short find_free_block()
{
    for (int i = 0; i <DATABLOCKNUM; i++)
        if (get_fat_value(i) == FREE)
            return i;

    return NOSPACE;
}

/* return the no of first block if success */
unsigned short alloc_blocks(int len)
{
    unsigned short first = find_free_block();
    unsigned short cur_block_no = first;
    if (cur_block_no == NULL)
            return NOSPACE;
    
    for (int i = 1; i < len; i++)
    {
        unsigned short next_block_no = find_free_block();

        if (next_block_no == NULL)
        {
            set_fat_value(cur_block_no, END);

            // free alread allocated blocks
            for (unsigned short i = first; i != END; )
            {   
                int t = i;
                i = get_fat_value(i);
                set_fat_value(t, FREE);
            }

            return NOSPACE;
        }

        set_fat_value(cur_block_no, next_block_no);
        cur_block_no = next_block_no;
    }

    set_fat_value(cur_block_no, END);
    return first;
}

void free_blocks(FCB *target)
{
    unsigned short first = target->first;

    for (unsigned short i = first; i != END; )
    {   
        int t = i;
        i = get_fat_value(i);
        set_fat_value(t, FREE);
    }
}

void format_blocks_to_fcbs(unsigned short start_no)
{
    FCB *pos = (FCB *)get_data_block_addr(start_no);
    fcb_for_each(pos, start_no)
    {
        pos->free = 1;
    }
}

FCB *find_fcb_by_name(FCB* p_fcb, const char *name)
{
    unsigned short start_no = p_fcb->first;
    FCB *pos = (FCB *)get_data_block_addr(start_no);

    fcb_for_each(pos, start_no)
    {
        if (!pos->free && !strncmp(pos->filename, name, 8))
            return pos;
    }
    
    return NULL;
}

/* return FCB located by dir, and store its absolute path in new_dir */
FCB *find_fcb_by_dir(const char *dir, char *new_dir)
{
    memset(new_dir, '\0', sizeof(new_dir));

    // root
    if (!strncmp(dir, "/", 80))
    {
        strncpy(new_dir, "/", 80);
        return &(superblock->root);
    }
        

    // remove final '/'
    int len = strnlen(dir, 80);
    if (dir[len - 1] == '/')
        len--;
    
    // determine start from where
    int i;
    FCB *ret; 
    if (dir[0] == '/')  // start from /
    {
        ret = &(superblock->root);
        strncat(new_dir, "/", 80);
        i = 1;
    }
    else    // start from curdir
    {
        ret = curfcb;
        strncpy(new_dir, curdir, 80);
        i = 0;
    }

    int j = 0;
    char tmp[8];
    memset(tmp, '\0', 8);

    for (; i <= len; i++)
    {
        if (i == len || dir[i] == '/')
        {
            if (!strncmp(tmp, "..", 8))
            {
                ret = parent_fcb(ret);
                if (ret == NULL)
                    return NULL;

                int new_dir_len = strnlen(new_dir, 80);
                if (new_dir_len != 1)
                {
                    new_dir[new_dir_len - 1] = '\0';
                    for (int k = new_dir_len - 2; k >= 0; k--)
                    {
                        if (new_dir[k] == '/')
                            break;
                        else
                            new_dir[k] = '\0';
                    }
                }
            }
            else if (!strncmp(tmp, ".", 8)){}
            else
            {
                ret = find_fcb_by_name(ret, tmp);
                if (ret == NULL)
                    return NULL;
                strncat(new_dir, ret->filename, 80);
                strncat(new_dir, "/", 80 - strnlen(new_dir, 80));
            }

            j = 0;
            memset(tmp, '\0', 8);
        }
        else
            tmp[j++] = dir[i];
    }
    return ret;
}

FCB *find_free_fcb(FCB *p_fcb)
{
    unsigned short start_no = p_fcb->first;
    FCB *pos = (FCB *)get_data_block_addr(start_no);
    fcb_for_each(pos, start_no)
        if (pos->free)
            return pos;

    // no more fcb entry, apply for anthoer free block
    if (start_no == END)
    {
        unsigned char new_block_no = append_file(p_fcb, 1);
        if (new_block_no == NOSPACE)
            return NULL;
        else
        {   //format new block to free fcb array
            format_blocks_to_fcbs(new_block_no);
            
            // return the first fcb block of the new block
            return (FCB *)get_data_block_addr(new_block_no);
        } 
    }

    return NULL;
}

void moveto_next_fcb(FCB *&pos, unsigned short &start_no)
{   
    unsigned char endfcb = BLOCKSIZE / sizeof(FCB) - 1;
    if (pos - (FCB *)get_data_block_addr(start_no) == endfcb)
    {
        start_no = get_fat_value(start_no);
        pos = (FCB *)get_data_block_addr(start_no);
    }
    else
        pos++;
}

FCB *parent_fcb(FCB *child)
{
    FCB *pos = find_fcb_by_name(child, "..");
    unsigned short start_no = pos->first;

    FCB *pos2 = find_fcb_by_name(pos, "..");
    unsigned short start_no2 = pos2->first;

    if (start_no == start_no2)
    {
        return &(superblock->root);
    }
    else
    {
        FCB *pos3 = (FCB *)get_data_block_addr(start_no2);
        fcb_for_each(pos3, start_no2)
        {
            if (pos3->first == start_no)
                return pos3;
        }
    }

    return NULL;
}

/*if success, return the first no of new blocks; if fail, return NOSPACE*/
unsigned short append_file(FCB *file, int block_num)
{
    unsigned short new_blocks = alloc_blocks(block_num);

    if (new_blocks == NOSPACE)
        return NOSPACE;
    else
    {
        unsigned short tail = file->first;
        
        while (get_fat_value(tail) != END)
        {
            tail = get_fat_value(tail);
        }

        set_fat_value(tail, new_blocks);
        
        return new_blocks;
    }
   
}

void split_path(const char *path, char *dir, char *file)
{
    memset(dir, '\0', sizeof(dir));
    memset(file, '\0', sizeof(file));

    // root
    if (!strncmp(path, "/", 80))
    {
        strncpy(dir, "/", 80);
        return;
    }

    // remove final '/' if have
    int len = strnlen(path, 80);
    if (path[len - 1] == '/')
        len--;

    // find where filename start
    int file_start = len - 1;
    while (file_start >= 0 && path[file_start] != '/')
    {
        file_start--;
    }
    file_start++;

    // get dir
    int dir_start;
    if (path[0] == '.' || path[0] == '.' && path[1] =='.' ||path[0] == '/')
        dir_start = 0;
    else
    {
        strncpy(dir, "./", 80);
        dir_start = 2;
    }
    for (int i = 0; i < file_start; i++)
    {
        dir[i + dir_start] = path[i];
    }

    // get file
    for (int i = file_start; i < len; i++)
    {
        file[i - file_start] = path[i];
    }
    
}

void startsys();
void exitsys();
void format();
int mkdir(const char *dirname);
int cd(const char *dirname);
void ls();
int create(const char *dirname);
int rm(const char *dirname);
int open(const char *dirname);
void my_close(int fd);
int write(int fd);
int do_write(int fd, char *buf, int len, unsigned short block_no);
int read(int fd, int len);
int do_read(int fd, int len, char *buf, unsigned short block_no);

int main ()
{
    startsys();
    char cmd[20];
    char dirname[8];
    memset(cmd, '\0',20);
    memset(dirname,'\0',8);
    printf ("%s > ", curdir);
    while(scanf("%s", cmd))
    {
        if (!strncmp(cmd, "format", 20))
        {
            format();
        }

        if (!strncmp(cmd, "exit", 20))
        {
            exitsys();
            break;
        }
        if (!strncmp(cmd, "mkdir", 20))
        {
            scanf("%s",dirname);
            mkdir(dirname);
        }
        if (!strncmp(cmd, "ls", 20))
            ls();
        if (!strncmp(cmd, "cd", 20))
        {
            scanf("%s",dirname);
            cd(dirname);
        }
        if (!strncmp(cmd, "rmdir", 20))
        {
            scanf("%s",dirname);
            rmdir(dirname);
        }
        if (!strncmp(cmd, "create", 20))
        {
            scanf("%s",dirname);
            create(dirname);
        }
        if (!strncmp(cmd, "rm", 20))
        {
            scanf("%s",dirname);
            rm(dirname);
        }
        if (!strncmp(cmd, "open", 20))
        {
            scanf("%s",dirname);
            int fd = open(dirname);
            printf("%d\n", fd);
        }
        if (!strncmp(cmd, "close", 20))
        {
            int fd;
            scanf("%d",&fd);
            my_close(fd);
        }
        if (!strncmp(cmd, "write", 20))
        {
            int fd;
            int len;
            scanf("%d",&fd);
            len = write(fd);
            printf("write: %d\n", len);
        }
        if (!strncmp(cmd, "read", 20))
        {
            int fd, len;
            scanf("%d %d",&fd, &len);
            len = read(fd, len);
            printf("\nread: %d\n", len);
        }

        printf ("%s > ", curdir);
        memset(cmd, '\0',20);
        memset(dirname,'\0',8);
    }
}

void startsys()
{
    // apply for 1M vdisk
    vdisk = (unsigned char *)malloc(DISKSIZE);
    memset(vdisk, 0, DISKSIZE);

    printf("File system loading...\n");

    // open a file as a binary file for input operation
    FILE *fp = fopen("FS.FAT", "rb");

    if (fp)
    {
        fseek(fp,0L,SEEK_SET);   // set pos indicator to the beginning of file
        fread(vdisk, DISKSIZE, 1, fp);  // read all to vdisk

        /* initialization */
        initialization();
    }
    else
    {
        format();
    }
    
    fclose(fp); // finished, close

    
}

void exitsys()
{
    printf("system closing...\n");

    free(vdisk);
    FILE *fp;
    fp = fopen("FS.FAT", "wb");// localfile文件名  
    fseek(fp, 0L, SEEK_SET);
    fwrite(vdisk, DISKSIZE, 1, fp);
    fclose(fp);

    printf("save file system to FS.FAT\n");

}

void format()
{
    /* Initialize SuperBlock */
    superblock = (SUPERBLOCK *)vdisk;
    superblock->fat_base = SUPERBLOCKNUM;
    superblock->fat2_base = SUPERBLOCKNUM + FATBLOCKNUM;
    superblock->data_block_base = SUPERBLOCKNUM + 2 * FATBLOCKNUM;
    strncpy(superblock->information, "simple file system supported by FAT", 200);

    /* Initialize FAT */
    fat_base = (FAT *)(vdisk + superblock->fat_base * BLOCKSIZE);
    fat2_base = (FAT *)(vdisk + superblock->fat2_base * BLOCKSIZE);
    for (int i = 0; i < DATABLOCKNUM; i++)
    {
        set_fat_value(i, FREE);
    }

    /* setup root */
    FCB *root =  &(superblock->root);
    set_fcb(root, 0, time(NULL), "root", ROOTSTARTBLOCK, 0, 0);
    
    int i;
    for (i = ROOTSTARTBLOCK; i < ROOTBLOCKNUM - 1; i++)
    {
        set_fat_value(i, i+1);
    }
    set_fat_value(i, END);

    data_block_base = (unsigned char *)(vdisk + superblock->data_block_base * BLOCKSIZE);
    format_blocks_to_fcbs(ROOTSTARTBLOCK);

    /* make special entry "." */
    FCB *dot_fcb = find_free_fcb(root);
    set_fcb(dot_fcb, 0, time(NULL), ".", ROOTSTARTBLOCK, 0, 0);
    fcb_len_add(root, sizeof(FCB));

    /* make special entry ".." */
    FCB *dotdot_fcb = find_free_fcb(root);
    set_fcb(dotdot_fcb, 0, time(NULL), "..", ROOTSTARTBLOCK, 0, 0);
    fcb_len_add(root, sizeof(FCB));

    initialization();
}

int mkdir(const char *dirname)
{
    char dir[80];
    char tmp[80];
    char file[8];
    split_path(dirname, dir, file);

    FCB *fcb = find_fcb_by_dir(dir, tmp);
    if (fcb == NULL)
        return -1;

    //check name
    if (find_fcb_by_name(fcb, file) != NULL)
        return -2;  // duplicate name

    //find free FCB
    FCB *free_fcb = find_free_fcb(fcb);
    if (free_fcb == NULL)
        return -3;  //no more space available

    unsigned short first = alloc_blocks(1);
    set_fcb(free_fcb, 0, time(NULL), file, first, 0, 0);
    format_blocks_to_fcbs(first);
    fcb_len_add(fcb, sizeof(FCB));
    //fcb_len_add(curfcb, BLOCKSIZE);

    /* make special entry "." */
    FCB *dot_fcb = find_free_fcb(free_fcb);
    set_fcb(dot_fcb, 0, time(NULL), ".", free_fcb->first, 0, 0);
    fcb_len_add(free_fcb, sizeof(FCB));

    /* make special entry ".." */
    FCB *dotdot_fcb = find_free_fcb(free_fcb);
    set_fcb(dotdot_fcb, 0, time(NULL), "..", fcb->first, 0, 0);
    fcb_len_add(free_fcb, sizeof(FCB));

    return 1;
}

int rmdir(const char *dirname)
{
    char tmp[80];
    FCB *target = find_fcb_by_dir(dirname, tmp);

    if (target->length > 2 * sizeof(FCB))
        return -2;  // have sundir

    if ( target == NULL)
    {
        return -1;  // name not found
    }

    free_blocks(target);
    target->free = 1;

    return 1;
}

int cd(const char *dirname)
{
    char new_dir[80];
    FCB *des = find_fcb_by_dir(dirname, new_dir);

    if ( des == NULL)
    {
        return -1;  //  name not found
    }

    curfcb = des;
    strncpy(curdir, new_dir, 80);
    return 1;
}

void ls()
{
    struct tm *timeinfo;

    unsigned short start_no = curfcb->first;
    FCB *pos = (FCB *)get_data_block_addr(start_no);

    printf("%s\t%s\t%s\t%s\n","filename", "attr", "len", "borntm");

    fcb_for_each(pos, start_no)
    {
        if (pos->free == 0)
        {
            const time_t tmp = pos->time;
            timeinfo = localtime(&tmp);
            printf("%s\t\t%d\t%d\t%s",pos->filename, pos->attribute, pos->length, asctime(timeinfo));
        }
    }
}

int create(const char *dirname)
{
    char dir[80];
    char tmp[80];
    char file[8];
    split_path(dirname, dir, file);

    FCB *fcb = find_fcb_by_dir(dir, tmp);
    if (fcb == NULL)
        return -1;

    //check name
    if (find_fcb_by_name(fcb, file) != NULL)
        return -2;  // duplicate name

    //find free FCB
    FCB *free_fcb = find_free_fcb(fcb);
    if (free_fcb == NULL)
        return -3;  //no more space available

    unsigned short first = alloc_blocks(1);
    set_fcb(free_fcb, 1, time(NULL), file, first, 0, 0);
    fcb_len_add(fcb, BLOCKSIZE);

    return 1;
}

int rm(const char *dirname)
{
    char tmp[80];
    FCB *target = find_fcb_by_dir(dirname, tmp);

    if ( target == NULL)
    {
        return -1;  //  name not found
    }

    useropen_for_each()
    {
        if (openfilelist[i].free != 0 && openfilelist[i].fcb == target)
            return -2;  // currenly being open
    }

    free_blocks(target);
    target->free = 1;

    return 1;
}

int open(const char *dirname)
{
    char dir[80];
    char tmp[80];
    char file[8];
    split_path(dirname, dir, file);

    FCB *fcb = find_fcb_by_dir(dir, tmp);
    if (fcb == NULL)
        return -1;

    //check name
    FCB *file_fcb = find_fcb_by_name(fcb, file);
    if (file_fcb == NULL)
        return -2;  // name not found
    
    useropen_for_each()
    {
        if (openfilelist[i].free == 0
            &&!strncmp(openfilelist[i].filename, file, 8)
            && !strncmp(openfilelist[i].dir,  tmp, 80)
            )
            return -3;  // alread opened
    }

    int fd;

    useropen_for_each()
    {
        if (openfilelist[i].free == 1)
        {
            //copy info of file_fcb into openfile
            strncpy(openfilelist[i].filename, file_fcb->filename, 8);
            openfilelist[i].attribute = file_fcb->attribute;
            openfilelist[i].time = file_fcb->time;
            openfilelist[i].first = file_fcb->first;
            openfilelist[i].length = file_fcb->length;
            openfilelist[i].fcb = file_fcb;
            strncpy(openfilelist[i].dir, tmp, 80);
            openfilelist[i].count = 0;
            openfilelist[i].fcbstate = 0;
            openfilelist[i].free = 0;
            fd = i;
            break;
        }
    }

    return fd;
}

void my_close(int fd)
{
    if (fd < 0 || fd >= MAXOPENFILE || openfilelist[fd].free == 1)
        return;//invalid fd

    openfilelist[fd].free = 1;

    if (openfilelist[fd].fcbstate == 1) // alread changed
    {
        openfilelist[fd].fcb->length = openfilelist[fd].length;
    }

    return;
}

int write(int fd)
{
    
    // check fd
    if (fd < 0 || fd >= MAXOPENFILE || openfilelist[fd].free == 1)
    {
        return -1;  //invalid fd
    }

    // chose write mode
    printf("chose your write mode: \n");
    char mode;
    scanf("%s", &mode);
    if (mode == 'w')    //override all
        openfilelist[fd].count = 0;
    else if (mode == 'r')
    {}
    else if (mode == 'a') // add
        openfilelist[fd].count = openfilelist[fd].length;

    // get buf
    char* buf = (char *)malloc(BLOCKSIZE);

    // find no of the block
    int no = openfilelist[fd].count / BLOCKSIZE;
    unsigned short block_no = openfilelist[fd].first;
    for (int i = 0; i < no; i++)
    {
        block_no = next_data_block_no(block_no);
    }


    // input
    printf("please input(end up with \\n^Z): \n");

    int i = openfilelist[fd].count % BLOCKSIZE;
    int len = 0;
    int ret = 0;
    getchar();

    buf[i] = getchar();
    len++;
    while (1)
    {

        if (buf[i] == EOF)
        {
            do_write(fd, buf, len - 2, block_no); //not include final \n^Z
            ret += len - 2;
            break;
        }

        if ((i + 1) % BLOCKSIZE == 0)
        {
            do_write(fd, buf, len, block_no);
            ret += len;
            len = 0;
            i = -1;
            
            unsigned short old_block = block_no;
            block_no = next_data_block_no(block_no);
            if (block_no == END)
            {
                unsigned short new_block = alloc_blocks(1);
                if (new_block == NOSPACE)
                    break; // no more space
                else
                {
                    next_data_block_no(old_block) = new_block;
                    block_no = new_block;
                }
            }
        }

        buf[++i] = getchar();
        len++;
    }

    free(buf);

    openfilelist[fd].length = openfilelist[fd].count;
    openfilelist[fd].fcbstate = 1;

    return ret;
}

int do_write(int fd, char *buf, int len, unsigned short block_no)
{
    int start_i = openfilelist[fd].count % BLOCKSIZE;
    char *base = (char *)get_data_block_addr(block_no);

    for (int i = start_i; i < start_i + len; i++)
    {
        *(base + i) = buf[i];
    }

    openfilelist[fd].count += len;
    return 1;
}

int read(int fd, int len)
{
    //  check fd
    if (fd < 0 || fd >= MAXOPENFILE || openfilelist[fd].free == 1)
    {
        return -1;  //invalid fd
    }

    // get buf
    char *buf = (char *)malloc(BLOCKSIZE);

    if (len > openfilelist[fd].length)
        len = openfilelist[fd].length;

    openfilelist[fd].count = 0;

    unsigned short block_no = openfilelist[fd].first;
    int num = len / BLOCKSIZE;
    for (int i = 0; i < num; i++)
    {
        do_read(fd, BLOCKSIZE, buf, block_no);
        
        for (int k = 0; k < BLOCKSIZE; k++)
            putchar(*(buf + k));
        
        block_no = next_data_block_no(block_no);
        if (block_no == END)
            return -2;  // len is too long
    }

    len = len % BLOCKSIZE;
    do_read(fd, len, buf, block_no);
    for (int k = 0; k < len; k++)
            putchar(*(buf + k));
    
    free(buf);
    return openfilelist[fd].count;
}

int do_read(int fd, int len, char *buf, unsigned short block_no)
{
    char *base = (char *)get_data_block_addr(block_no);

    for (int i = 0; i < len; i++)
    {
        buf[i] = *(base + i);
    }

    openfilelist[fd].count += len;
    return 1;
}
