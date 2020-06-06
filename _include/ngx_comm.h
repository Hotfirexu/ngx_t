#ifndef __NGX_COMM_H__
#define __NGX_COMM_H__

#define _PKG_MAX_LENGTH 30000 //每个包的最大长度【包头 + 包体】不超过这个长度，

//收包状态定义

#define _PKG_HD_INIT_       0  //初始状态，准备接收数据包头
#define _PKG_HD_RECVING     1  //接收包头中，包头不完整，继续接收中
#define _PKG_BD_INIT        2  //包头刚好接收完，准备接收包体
#define _PKG_BD_RECVING     3  //接收包体中，包体不完整，继续接收中，处理后回到_PKG_HD_INIT状态

#define _DATA_BUFSIZE_      20  // 因为要先接收包头，定义一个固定大小的数组专门用来收包头，这个数组的大小一定要大于sizeof（COMM_PKG_HEAD）
                                // 

//结构定义
#pragma pack(1)  //对齐方式，1字节对齐【结构之间成员不做任何字节对齐：紧密排列在一起】

typedef struct _COMM_PKG_HEADER {
    unsigned short  pkgLen;     // 报文总长度【包头 + 包体】 -- 2字节，2字节可以表示的最大数字为6万多，完全足够了

    unsigned short  msgCode;    // 消息类型
    int             crc32;      // CRC校验

}COMM_PKG_HEADER, *LPCOMM_PKG_HEADER;

#pragma pack()   //取消指定对齐，恢复缺省对齐方式

#endif