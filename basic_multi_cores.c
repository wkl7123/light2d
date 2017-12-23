#include "svpng.inc"
#include <math.h> // fminf(), sinf(), cosf()
#include <stdlib.h> // rand(), RAND_MAX
#include <unistd.h>
#include <sys/types.h>  
#include <sys/ipc.h>  
#include <sys/shm.h>  

#define TWO_PI 6.28318530718f
#define N 64                    // 采样次数
#define MAX_STEP 10             // 最大追踪步长
#define MAX_DISTANCE 2.0f       // 最大追踪距离
#define EPSILON 1e-6f           // 0的float表示
#define PROCESS_NUM 4              // cpu核心数目
#define SHM_MODE    (SHM_R | SHM_W | IPC_CREAT) /* user read/write */  
#define SHM_KEY     0x78


/*******************************************************
Function circleSDF:  二维圆盘光源
Description: 计算点(x,y)与光源(圆心在(cx,cy), 半径为r)的距离

Input:
float x  点的x坐标
float y  点的y坐标
float cx 圆盘圆心横坐标
float cy 圆盘圆心纵坐标
float r  圆盘半径

Return: 距离(可为负值)
********************************************************/
float circleSDF(float x, float y, float cx, float cy, float r) {
    float ux = x - cx, uy = y - cy;
    return sqrtf(ux * ux + uy * uy) - r;
}

/************************************************************************
Function trace: 光线步进函数
Description: 代表从 \mathbf{o} 位置从单位矢量 \hat{\mathbf{d}} 方向接收到的光

const MAX_STEP      最大步长
const MAX_DISTANCE  最大光线追踪距离

Input:
float ox o点的x坐标
float oy o点的y坐标
float dx 单位向量的x分量 cos(\theta)
float dy 单位向量的y分量 sin(\theta)

Return: 从一个角度接收到的光
*************************************************************************/
float trace(float ox, float oy, float dx, float dy) {
    float t = 0.0f;
    for (int i = 0; i < MAX_STEP && t < MAX_DISTANCE; i++) {
        float sd = circleSDF(ox + dx * t, oy + dy * t, 0.5f, 0.5f, 0.1f);
        if (sd < EPSILON)
            return 2.0f;
        t += sd;
    }
    return 0.0f;
}

/************************************************************************
Function sample: 采样函数
Description: 点(x,y)通过(蒙特卡洛 均匀 抖动)采样获得当前点收到的光的平均値

const RAND_MAX 最大的随机数, 在stdlib.h中有定义

Input: 
float x 点的横坐标
float y 点的纵坐标

Return: 点平均接收到的光
*************************************************************************/
float sample(float x, float y) {
    float sum = 0.0f;
    for (int i = 0; i < N; i++) {
        // float a = TWO_PI * rand() / RAND_MAX;               // 随机采样
        // float a = TWO_PI * i / N;                           // 均匀采样
        float a = TWO_PI * (i + (float)rand() / RAND_MAX) / N; // 抖动采样
        sum += trace(x, y, cosf(a), sinf(a));
    }
    return sum / N;
}

/************************************************************************
Function draw: 画图

Input:
float width  图像宽带
float height 图像高度
char[] filename 图像文件路径
*************************************************************************/
void draw(int width, int height, char *filename)
{
    unsigned char img[width * height * 3];
    unsigned char* p = img;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++, p += 3)
            p[0] = p[1] = p[2] = (int)(sample((float)x / width, (float)y / height) * 255.0f);
    svpng(fopen(filename, "wb"), width, height, img, 0);
}

/************************************************************************
Function draw: 画图(多进程版本)

Input:
float width  图像宽带
float height 图像高度
char[] filename 图像文件路径
*************************************************************************/
void draw_paralell(int width, int height, char *filename)
{
    int per = floor(height/PROCESS_NUM);
    int line[PROCESS_NUM];                // work start line num
    for(int i = 0; i < PROCESS_NUM; i++)
    {
        line[i] = per * i;
    }

    // unsigned char* img = malloc(sizeof(unsigned char) * width * height * 3);
    int n = 0;
    int y0 = line[n];
    // unsigned char*  p = img;
    int p = 0;
    for(int j = 0; j < PROCESS_NUM; j++)
    {
        if(fork() == 0)
        {
            int shmid;  
            unsigned char *shmptr;  
            if ((shmid = shmget(SHM_KEY, width * height * sizeof(int), SHM_MODE | IPC_CREAT)) < 0)  
                perror("shmget");  
            if ((shmptr = shmat(shmid, 0, 0)) == (void *) -1)  
                perror("shmat");  

            int ymax = (n == PROCESS_NUM) ? height : (y0 + per);
            for (int y = y0; y < ymax; y++)
            {
                for(int x = 0; x < width; x++, p += 3)
                {
                    shmptr[p] = shmptr[p+1] = shmptr[p+2] = (int)(sample((float)x / width, (float)y / height) * 255.0f);
                    // printf("x=%f  y=%f sample=%f\n", (float)x / width, (float)y / height, sample((float)x / width, (float)y / height));
                }
            }
            exit(0);

        } else {
            n += 1;
            y0 = line[n];
            p = y0 * width * 3;
        }
    }

    for (int k = 0; k < PROCESS_NUM; k++)
    {
        wait(0);
    }

    int shmid;  
    unsigned char *shmptr;  
    // 从共享内存中取出数据
    if ( (shmid = shmget(SHM_KEY, width * height * sizeof(int), SHM_MODE)) < 0)  
        perror("shmget");  
  
    if ( (shmptr = shmat(shmid, 0, 0)) == (void *) -1)  
        perror("shmat");  

    svpng(fopen(filename, "wb"), width, height, (unsigned char *)shmptr, 0);

    shmctl(shmid, IPC_RMID, 0); // 删除共享内存
    
}


int main() {
    // draw(512, 512, "basic_haha.png");
    draw_paralell(512, 512, "basic_hehe.png");
    printf("finished\n");
}
