#include<fstream>
#include <iostream>
#include<ctime>
#include<cstdio>
#include<queue>
#include<vector>
#include<map>
using namespace std;
const int KVDB_OK = 0;
const int KVDB_INVALID_AOF_PATH = 1;
const int KVDB_INVALID_KEY = 2;
const int KVDB_NO_SPACE_LEFT_ON_DEVICES = 3;
struct dlen
{
    int klen;
    int vlen;
    dlen(int kl, int vl)
    {
        klen = kl;
        vlen = vl;
    }
};
struct figure
{
    string key;
    string value;
    figure(string k, string v)
    {
        key = k;
        value = v;
    }
};
struct Node
{
    string key;
    int interval;
    int currenttime;
    Node()
    {
        currenttime = time(NULL);
    }
    Node(string tk, int n, int cur) :key(tk), interval(n), currenttime(cur) {}
    friend bool operator>(const struct Node& n1, const struct Node& n2);
};
inline bool operator >(const struct Node& n1, const struct Node& n2)
{
    return n1.interval > n2.interval;
}
void setdata(dlen& d1, figure& d2, ofstream& out)
{
    cout << "Now writing the data to the file" << endl;
    if (d1.vlen == -1)
    {
        out.write((char*)&d1.klen, 4);
        out.write((char*)&d1.vlen, 4);
        out.write((char*)&d2.key[0], d1.klen);
    }
    else
    {
        out.write((char*)&d1.klen, 4);
        out.write((char*)&d1.vlen, 4);
        out.write((char*)&d2.key[0], d1.klen);
        out.write((char*)&d2.value[0], d1.vlen);
    }
}
string readdata(ifstream& in, int len)
{
    char* tv = new char[len + 1];
    in.read(tv, len);
    tv[len] = '\0';
    string t2 = string(tv);
    return t2;
}
int getoffset(ifstream& t, const string& key)
{
    int off=-1;
    if (t.is_open())
    {
        while (t.peek() != EOF)
        {
            int kl, vl;
            t.read((char*)&kl, 4);
            t.read((char*)&vl, 4);
            string t1 = readdata(t, kl);
            if (t1 == key)
            {
                if (vl != -1)
                {
                    off = t.tellg();
                    t.seekg(vl, ios::cur);
                }
                else
                    off = -1;
            }
            else
            {
                if (vl != -1)
                    t.seekg(vl, ios::cur);
            }
        }
    }
    return off;
}
struct hashnode
{
    string key;
    int offset;
    hashnode* next = NULL;
};
class hashtable
{
    hashnode** head;
    int n;
public:
    hashtable(int tn)
    {
        n = tn;
        create_hash_table(tn);
    }
    void create_hash_table(int tn)
    {
        n = tn;
        head = new hashnode * [n];
        for (int i = 0; i < n; i++)
            head[i] = NULL;
    }
    unsigned int hash_index(string key)
    {
        unsigned long h = 0;
        for (int i = 0; i < (int)key.length(); i++)
        {
            h += key[i];
        }
        return h % n;
    }
    void hash_insert(const string& key, const int &offset)
    {
        hashnode* node = new hashnode;
        node->key = key;
        node->offset = offset;
        int index = hash_index(key) % n;
        hashnode* p = new hashnode;
        p = head[index];
        int tt = hash_find(key);
        if (tt != -1)
            hash_set(key, offset);
        else
        {
            if (p != NULL)
            {
                node->next = p->next;
                p->next = node;
            }
            else
                //p = node;
                head[index] = node;
        }
    }
    void hash_delete(const string& key)
    {
        int index = hash_index(key) % n;
        hashnode* p = head[index];
        while (p)
        {
            if (p->key == key)
            {
                hashnode* q = p;
                if (p != head[index])
                    p = p->next;
                else
                    head[index] = p->next;
                q->next = NULL;
            }
            else
                p = p->next;
        }
    }
    void hash_set(const string& key, const int& offset)
    {
        int index = hash_index(key) % n;
        hashnode* p = head[index];
        while (p)
        {
            if (p->key == key)
            {
                p->offset = offset;
                //p->value = value;
                return;
            }
            p = p->next;
        }
        hashnode* q = new hashnode;
        q->key = key;
        q->offset = offset;
        //q->value = value;
        p = q;
    }
    int hash_find(const string& key)
    {
        int index = hash_index(key) % n;
        hashnode* p = head[index];
        while (p)
        {
            if (p->key == key)
            {
                return p->offset;
            }
            else
                p = p->next;
        }
        return -1;
    }
    void hash_writefile(ifstream &in,ofstream& t)
    {
        for (int i = 0; i < n; i++)
        {
            if (head[i] != NULL)
            {
                hashnode* p = head[i];
                while (p)
                {
                    in.seekg(p->offset, ios::beg);
                    int kl = (int)p->key.length();
                    in.seekg(-(kl + 4), ios::cur);
                    int vl;
                    in.read((char*)&vl, 4);
                    in.seekg(kl, ios::cur);
                    string tvalue = readdata(in, vl);
                    dlen d1(p->key.length(), vl);
                    figure d2(p->key,tvalue);
                    setdata(d1, d2, t);
                    p = p->next;
                }
            }
        }
    }
    void hash_change_offset(ifstream& t)
    {
        if (t.is_open())
        {
            while (t.peek()!=EOF)
            {
                int kl, vl;
                t.read((char*)&kl, 4);
                t.read((char*)&vl, 4);
                string t1 = readdata(t, kl);
                if (vl != -1)
                {
                    int offset = t.tellg();                   
                    hash_set(t1, offset);
                    t.seekg(vl, ios::cur);
                }
            }
        }
    }
};
string getcurtime()
{
    char tmp[100];
    time_t ptime;
    time(&ptime);
    struct tm nowtime;
    localtime_s(&nowtime, &ptime);
    strftime(tmp, sizeof(tmp), "Now is %Y-%m-%d %H:%M:%S", &nowtime);
    return tmp;
}
class Logger
{
public:
    enum log_level { debug, info, warning, error, fatal };
    enum log_target { file, terminal, file_and_terminal };
    ofstream out;//将日志输出到文件的流对象
    log_target target;//日志输出目标
    log_level level;//日志等级
    string path; //日志文件路径
    void display(string text, log_level reallevel)
    {
        string str;
        if (reallevel == debug)
            str = "[DEBUG]  ";
        else if (reallevel == info)
            str = "[INFO]  ";
        else if (reallevel == error)
            str = "[ERROR]  ";
        else if (reallevel == warning)
            str = "[WARNING]  ";
        else if (reallevel == fatal)
            str = "[FATAL]  ";
        else
            str = "";
        str += __FILE__;
        str += " ";
        string output = str + getcurtime() + ":" + text;
        if (level <= reallevel && target != file)
            cout << output << endl;
        if (target != terminal)
            out << output << endl;
    }

public:
    Logger()
    {
        target = terminal;
        level = debug;
        cout << "[Welcome]" << __FILE__ << " " << getcurtime() << ":" << "===Start logging===" << endl;
    }
    Logger(log_target real_target, log_level real_level, string tpath)
    {
        target = real_target;
        level = real_level;
        path = tpath;
        string temp = "";
        string dialog = temp + "[Welcome]" + __FILE__ + " " + getcurtime() + ":" + "===Start logging===\n";
        if (target == file || target == file_and_terminal)
        {
            out.open(path, ios::out | ios::app);
            out << dialog;
        }
        if (target == terminal || target == file_and_terminal)
        {
            cout << dialog;
        }
    }
    void Debug(string text)
    {
        display(text, debug);
    }
    void Info(string text)
    {
        display(text, info);
    }
    void Warning(string text)
    {
        display(text, warning);
    }
    void Error(string text)
    {
        display(text, error);
    }
    void Fatal(string text)
    {
        display(text, fatal);
    }
};
class KVDBHandler {
private:
    ofstream out;
    ifstream in;
    string filename;
    Logger* logger1;
    hashtable* table;
    priority_queue<Node, vector<Node>, greater<Node>> pq;
    map<string, vector<int>>mapTime;
public:
    KVDBHandler(const string db_file);
    int set(const string& key, const string& value);
    int get(const string& key, string& value);
    void del(const string& key);
    void purge();
    void expires(const string& key, int n);
    void update();
    void rebuild();
    ~KVDBHandler();
};
KVDBHandler::KVDBHandler(const string db_file)
{
    filename = db_file;
    logger1 = new Logger(Logger::file_and_terminal, Logger::debug, "result.log");
    table = new hashtable(10000);
    in.open(filename, ios::in);
    if (in.is_open())
    {
        while (in.peek() != EOF)
        {
            int kl, vl;
            in.read((char*)&kl, 4);
            in.read((char*)&vl, 4);
            string t1 = readdata(in, kl);
            if (vl != -1)
            {
                int offset = in.tellg();
                table->hash_insert(t1,offset);
                in.seekg(vl, ios::cur);
            }
            else
            {
                int temp = table->hash_find(t1);
                if (temp != -1)
                    table->hash_delete(t1);
            }
        }
    }
    in.close();
    //用一个文件存储过期的信息。
    in.open("record2.txt", ios::in);
    if (in.is_open())
    {
        while (in.peek() != EOF)
        {
            string tt = readdata(in, 4);
            int time;
            in.read((char*)&time, 4);
            if (time != -1)
            {
                vector<int> v1;
                v1.push_back(time);
                int curtime;
                in.read((char*)&curtime, 4);
                v1.push_back(curtime);
                if (mapTime.find(tt) != mapTime.end())
                    mapTime[tt] = v1;
                else
                    mapTime.insert(pair<string, vector<int>>(tt, v1));
            }
        }
    }
    in.close();
    map<string, vector<int>>::iterator it;
    for (it = mapTime.begin(); it != mapTime.end(); it++)
    {
        Node temp(it->first, it->second[0], it->second[1]);
        pq.push(temp);
    }
}
void KVDBHandler::purge()
{
    ofstream temp;
    temp.open("temp.txt", ios::out | ios::binary | ios::app);
    if (!temp)
        logger1->Warning("Error opening temp_file");
    in.open(filename, ios::in);
    if (!in)
        logger1->Warning("Error opening in_file");
    table->hash_writefile(in,temp);
    in.close();
    temp.close();
    if (remove(filename.c_str()) == 0)
        logger1->Info("Delete  file success");
    else
        logger1->Warning("Delete file fail");
    if (rename("temp.txt", filename.c_str()) == 0)
        logger1->Info("Rename file success");
    else
        logger1->Warning("Rename file fail");
    ifstream tempin;
    tempin.open(filename, ios::in);
    table->hash_change_offset(tempin);
    tempin.close();
    logger1->Info("Purge operation succeeded");
}
int KVDBHandler::set(const string& key, const string& value)
{
    if (key.length() == 0)
    {
        logger1->Warning("int KVDBHandler::set()---the key is invalid");
        return KVDB_INVALID_KEY;
    }
    else
    {
        out.open(filename, ios::out | ios::binary | ios::app);
        if (!out)
            logger1->Warning("Error opening file");
        dlen d1(key.length(), value.length());
        figure d2(key, value);
        if (out.is_open())
            setdata(d1, d2, out);
        out.flush();//@增加这个语句，将缓冲区的数据立刻写到文件中
        out.close();
        in.open(filename, ios::in);
        int off = getoffset(in, key);
        in.close();
        if(off!=-1)
            table->hash_insert(key,off);
        out.open("record2.txt", ios::out | ios::app | ios::binary);
        map<string, vector<int>>::iterator it;
        it = mapTime.find(key);
        if (it != mapTime.end())
        {
            if (out.is_open())
            {
                out.write((char*)&key[0], 4);
                int t = -1;
                out.write((char*)&t, 4);
            }
            mapTime.erase(it);
        }
        out.close();
        logger1->Info("int KVDBHandler::set()---Set operation succeeded");
        return KVDB_OK;
    }
}
void KVDBHandler::rebuild()
{
    while (!pq.empty())
    {
        pq.pop();
    }
    map<string, vector<int>>::iterator it;
    for (it = mapTime.begin(); it != mapTime.end(); it++)
    {
        Node temp(it->first, it->second[0], it->second[1]);
        pq.push(temp);
    }
}
int KVDBHandler::get(const string& key,string& value)
{
    if (key.length() == 0)
    {
        logger1->Warning("int KVDBHandler::get()---the key is invalid");
        return KVDB_INVALID_KEY;
    }
    else
    {
        int off = table->hash_find(key);
        if (off != -1)
        {
            in.open(filename, ios::in);
            if (in.is_open())
            {
                int kl = (int)key.length();
                in.seekg(off, ios::beg);
                in.seekg(-(kl + 4), ios::cur);
                int vl;
                in.read((char*)&vl, 4);
                in.seekg(kl, ios::cur);
                value=readdata(in, vl);
            }
            in.close();
        }
        else
            value = "";
        //获取数据，获取文件中最后一个关键词为key的value
        logger1->Info("int KVDBHandler::get()---Get operation succeeded");
        return KVDB_OK;
    }
}
void KVDBHandler::del(const string& key)
{
    dlen d1(key.length(), -1);
    figure d2(key, "");
    logger1->Info("Now ready to del");
    out.open(filename, ios::out | ios::binary | ios::app);
    if (!out)
        logger1->Warning("Error opening file");
    if (out.is_open())
        setdata(d1, d2, out);
    out.flush();
    out.close();
    int tt = table->hash_find(key);
    if (tt != -1)
        table->hash_delete(key);
    out.open("record2.txt", ios::out | ios::app | ios::binary);
    map<string, vector<int>>::iterator it;
    it = mapTime.find(key);
    if (it != mapTime.end())
    {
        if (out.is_open())
        {
            out.write((char*)&key[0], 4);
            int t = -1;
            out.write((char*)&t, 4);
        }
        mapTime.erase(it);
    }
    out.close();
    logger1->Info("Delete operation succeeded");
}
KVDBHandler::~KVDBHandler()
{
    if (out)
        out.close();
    if (in)
        in.close();
}
void KVDBHandler::expires(const string& key, int n)
{
    out.open("record2.txt", ios::out | ios::app | ios::binary);
    map<string, vector<int>>::iterator it;
    int cur_time = time(NULL);
    vector<int> v1;
    v1.push_back(n);
    v1.push_back(cur_time);
    it = mapTime.find(key);
    if (it != mapTime.end())
    {
        mapTime[key] = v1;
    }
    else
    {
        mapTime.insert(pair<string, vector<int>>(key, v1));
    }
    pq.push(Node(key, n, cur_time));
    out.write((char*)&key[0], 4);
    int time = n;
    out.write((char*)&time, 4);
    out.write((char*)&cur_time, 4);
    out.close();
}
void KVDBHandler::update()
{
    //遍历堆顶元素，将所有已过期的key删除
    ofstream outfile;
    outfile.open("record2.txt", ios::out | ios::app | ios::binary);
    while (!pq.empty())
    {
        Node temp = pq.top();
        map<string, vector<int>>::iterator it;
        it = mapTime.find(temp.key);
        if (it != mapTime.end() && mapTime[temp.key][0] == temp.interval)
        {
            int nowtime = time(NULL); //获取现在的时间
            if (nowtime >= temp.currenttime + temp.interval)
            {
                //删除
                mapTime.erase(it);
                outfile.write((char*)&temp.key[0], 4);
                int time = -1;
                outfile.write((char*)&time, 4);
                del(temp.key); //@删除文件里的对应key
            }
        }
        pq.pop();
    }
    outfile.close();
    rebuild();
}
int main() {
    string file;
    cout << "Enter the database you want to open:";
    cin >> file;
    KVDBHandler handler((const string)file);
    string op;
    while (true)
    {
        cin >> op;
        if (op == "set")
        {
            string key;
            string value;
            cout << "Enter the key and value you want to add:";
            cin >> key >> value;
            handler.set(key, value);
        }
        else if (op == "get")
        {
            handler.update();
            string key;
            string value="";
            cout << "Enter the key value you want to get:";
            cin >> key;
            handler.get(key, value);
            if (value == "")
                cout << "The value you get is null" << endl;
            else
                cout << "The value you get is :" << value << endl;
        }
        else if (op == "delete")
        {
            string key;
            cin >> key;
            cout << "Enter the key you want to delete" << endl;
            handler.del(key);
        }
        else if (op == "purge")
        {
            handler.purge();
        }
        else if (op == "expires")
        {
            int n;
            string key;
            cout << "Enter the key you want to expire:" << endl;
            cin >> key;
            cout << "Enter the time you want to set:" << endl;
            cin >> n;
            handler.expires(key, n);
        }
        else
            break;
    }
}
