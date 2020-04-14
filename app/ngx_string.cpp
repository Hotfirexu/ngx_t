#include <stdio.h>
#include <string.h>


//去除字符串尾部空字符
void Rtrim(char *string) {
    size_t len = 0;
    if (string == NULL) {
        return;
    }

    len = strlen(string);
    while (len > 0 && string[len - 1] == ' ')
        string[--len] = 0;
    return;
}

//去除字符串首部空字符
void Ltrim(char *string) {
    size_t len = 0;
    len = strlen(string);
    char *p_tmp = string;
    if ((*p_tmp) != ' ') //不是以空格开头
        return;
    

    while ((*p_tmp) != '\0') { //找到第一个不为空格的
        if ((*p_tmp) == ' ')
            p_tmp ++;
        else
            break;
    }

    if ((*p_tmp) == '\0') {//全是空格
        *string = '\0';
        return;
    }

    char *p_tmp2 = string;
    while ((*p_tmp) != '\0') {
        (*p_tmp2) = (*p_tmp);
        p_tmp ++;
        p_tmp2 ++;
    }
    (*p_tmp2) = '\0';
    return;
}