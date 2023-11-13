#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/inotify.h>

#define MSG(args...) printf(args)
#define MSG_I(args...) printf(args)
#define MSG_W(args...) printf(args)
#define MSG_E(args...) printf(args)

int fd;
int wd;

// 定义操作的gpio名字数组
char gpioname[60];

static int gpio_export(int pin);
static int gpio_unexport(int pin);
static int gpio_direction(int pin, int dir);
static int gpio_write(int pin, int value);
static int gpio_read(int pin);

// 参数： 需要导出的gpio号，需要存在且有效
static int gpio_export(int pin)
{
        char buffer[64];
        int len;
        int fd;
// 先检查是否gpio已经export了，防止重复export
        char gpio_pin[64];
        snprintf( gpio_pin, sizeof(gpio_pin), "/sys/class/gpio/gpio%d", pin );
        if( access( gpio_pin, F_OK ) == 0 ){
                MSG_I("%s it already exists\n", gpio_pin );
                return 0;
        }

// 导出需要操作的gpio号，效果等同于shell 命令的 echo xxx > /sys/class/gpio/export
        fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) {
                MSG("Failed to open export for writing!\n");
                return(-1);
        }

        len = snprintf(buffer, sizeof(buffer), "%d", pin);
        if (write(fd, buffer, len) < 0) {
                MSG("Failed to export gpio!\n");
                return -1;
        }

        close(fd);
        return 0;
}

// export的反向操作
static int gpio_unexport(int pin)
{
        char buffer[64];
        int len;
        int fd;

        fd = open("/sys/class/gpio/unexport", O_WRONLY);
        if (fd < 0) {
                MSG("Failed to open unexport for writing!\n");
                return -1;
        }

        len = snprintf(buffer, sizeof(buffer), "%d", pin);
        if (write(fd, buffer, len) < 0) {
                MSG("Failed to unexport gpio!");
                return -1;
        }
        close(fd);
        return 0;
}

//设置gpio方向， 一般不需要，GPIO默认方向是OK的
static int gpio_direction(int pin, int dir)
{
        static const char dir_str[] = "in\0out";
        char path[64];
        int fd;

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

        fd = open(path, O_WRONLY);
        if (fd < 0) {
                MSG("Failed to open gpio direction for writing!\n");
                return -1;
        }

        if (write(fd, &dir_str[dir == 0 ? 0 : 3], dir == 0 ? 2 : 3) < 0) {
                MSG("Failed to set direction!\n");
                return -1;
        }

        close(fd);
        return 0;
}

// 修改gpiovalue，一般用于向DO端口输出的时候调用
static int gpio_write(int pin, int value)
{
        static const char values_str[] = "01";
        char path[64];
        int fd;

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
        fd = open(path, O_WRONLY);
        if (fd < 0) {
                MSG("Failed to open gpio value for writing!\n");
                return -1;
        }

        if (write(fd, &values_str[value == 0 ? 0 : 1], 1) < 0) {
                MSG("Failed to write value!\n");
                return -1;
        }

        close(fd);
        return 0;
}

// 读取gpiovalue，可用于获取gpio的值，可以用于读取输入端口DI的值。
static int gpio_read(int pin)
{
        char path[64];
        char value_str[3];
        int fd;

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
        fd = open(path, O_RDONLY);
        if (fd < 0) {
                MSG("Failed to open gpio value for reading!\n");
                return -1;
        }

        if (read(fd, value_str, 3) < 0) {
                MSG("Failed to read value!\n");
                return -1;
        }

        close(fd);

        return (atoi(value_str));
}

/* edge 表示中断的触发方式，edge文件有如下四个值："none", "rising", "falling"，"both"。
none表示引脚为输入，不是中断引脚  		--------------------> 这个对于输入端口， 我们的设定是作为输入引脚
rising表示引脚为中断输入，上升沿触发
falling表示引脚为中断输入，下降沿触发
both表示引脚为中断输入，边沿触发
这个文件节点只有在引脚被配置为输入引脚的时候才存在。 当值是none时可以通过如下方法将变为中断引脚
echo "both" > edge;对于是both,falling还是rising依赖具体硬件的中断的触发方式。此方法即用户态gpio转换为中断引脚的方式
*/
static int gpio_edge(int pin, int edge)
{
        const char dir_str[][9] = {"none\0","rising\0","falling\0","both"};
        char path[64];
        int fd;

        snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/edge", pin);
        fd = open(path, O_WRONLY);
        if (fd < 0) {
                MSG("Failed to open gpio edge for writing!\n");
                return -1;
        }

        if (write(fd, &dir_str[edge], strlen(dir_str[edge])) < 0) {
                MSG("Failed to set edge!\n");
                return -1;
        }

        close(fd);
        return 0;
}

/*
  使用system()函数调用系统命令，
*/
void call_system(char *cmd)
{
        if(NULL != cmd)
        {
                system(cmd);
        }
}


void call_shutdown()
{
		/* 使用cmd_test 打印一条消息， 不真正关机，调试的时候非常有用 */
        char cmd_test[]="echo doing shutdown test!";
        char cmd[]="poweroff -f";
        call_system(cmd);
        return;
}


// 和gpio_read等价的函数
void date_value_read()
{
        int ret = 0;
        char value[3]={0};
        int fd=open(gpioname,O_RDONLY);
        if(fd <=0 ){
                perror("open");
                return;
        }

        ret = read(fd,value,2);
        if (ret <= 0) {
                perror("%s, read");
        }
        // printf("%d bytes read, value:%s\n",ret, value);

// 检测到文件的值是1 调用关机指令
        if(0 == strncmp(value,"0",1)){
                //printf("value was 0, do nothing\n");
                ; // donothing
        } else if (0 == strncmp(value,"1",1)) {
                printf("value was 1, now -> calling shutdown !\n");
                call_shutdown();
                close(fd);
                return;
        } else {
                printf("unexpected value get, meet error\n");
                close(fd);
                return;
        }

        close(fd);
        return;
}

/* 调用 inotify 系列的函数 */
void inotify_fun(struct inotify_event *event, void *puser)
{
        if ( event->mask & IN_MODIFY )
        {
                printf( "The file %s was modified.\n", event->name );
                date_value_read();
        }
        return;
}

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024*(EVENT_SIZE+16))

#define DI_0 421
#define DI_1 419
#define DI_2 264
#define DI_3 265
#define DO_0 266
#define DO_1 267

void main()
{
        int ret;
#if 1
		// 本例使用DI_0做参考
        gpio_export(DI_0);
        gpio_direction(DI_0, 0);
        gpio_edge(DI_0, 0);

        fd = inotify_init1(0);
        if(fd == -1)
        {
                perror("inotify_init1");
                exit(0);
        }

        sprintf(gpioname,"/sys/class/gpio/gpio%d/value",DI_0);
        wd = inotify_add_watch(fd, gpioname, IN_CREATE|IN_DELETE|IN_MODIFY);
        if(wd == -1)
        {
                perror("inotify_add_watch");
                exit(0);
        }
#else
		// 这部分的函数是 #if 1 控制的else 部分，不会被执行，将#if 1 改成 #if 0.这里的代码生效，
		// 使用本地文件调试取代从gpio里面获取值
        fd = inotify_init1(0);
        if(fd == -1)
        {
                perror("inotify_init1");
                exit(0);
        }

        sprintf(gpioname,"./%d_value",DI_0);
        call_system("echo 1 > 421_value");

        wd = inotify_add_watch(fd, gpioname, IN_MODIFY);
        if(wd == -1)
        {
                perror("inotify_add_watch");
                exit(0);
        }
#endif

        fd_set rfd;
        struct timeval tv;
        char bufevent[BUF_LEN] = {0};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        for(;;)
        {
                int retval;
                FD_ZERO(&rfd);
                FD_SET(fd,&rfd);
                retval = select(fd+1, &rfd, NULL, NULL, &tv);
                if(retval == 0)
                {
                        date_value_read();
                        usleep(500000);
                        continue;
                }
                else if(retval == -1)
                {
                        return;
                }
//              printf("this is a cycle\n");
                int len = read(fd, bufevent, BUF_LEN);
                int index = 0;
                while(index < len)
                {
                        struct inotify_event *event = (struct inotify_event*)&bufevent[index];
                        {
                                inotify_fun(event,NULL);
                                index += EVENT_SIZE +  event->len;
                        }
                }
        }

        close(wd);
        close(fd);
}