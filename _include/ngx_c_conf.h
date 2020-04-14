#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

#include <vector>
#include "ngx_global.h"


//类名前面加一个大写的C
class CConfig {

private:
    CConfig();
public:
    ~CConfig();

public:
    static CConfig* GetInstance() {
        if (m_instance == NULL) {
            //锁
            if (m_instance == NULL) {
                m_instance = new CConfig();
                static CGarhuishou cl;
            }
        }
        return m_instance;

    }

    class CGarhuishou { //类中套类，用于释放对象
        public:
            ~CGarhuishou() {
                if (CConfig::m_instance) {
                    delete CConfig::m_instance;
                    CConfig::m_instance = NULL;
                }
            }
    };
public:
    bool Load(const char *pconfName);
    const char *GetString(const char *p_itemname);
    int GetIntDefault(const char *p_itemname, const int def);
private:
    static CConfig *m_instance;
    
public:
    std::vector<LPCConfItem> m_ConfigItemList;
};



#endif