# DB2
### 一.简介

1.**需求&背景&介绍**    

* 在这个信息呈爆炸式增长的时代，海量数据的存储和操作显得尤为重要。以前还有关系型数据库能够满足所需的要求，但随着数据操作的复杂性的日益提高，关系型数据库的一些缺点也逐渐显露出来。

* 近几年来Key-value数据库开始走进人们的视野，并已在一些应用中彰显出其优势。Key-value数据库是以Key-value数据存储为基础的数据库管理系统。Key-value数据存储技术以键值对的数据模型存储数据，并能提供持久化机制和数据同步功能。  

* 数据模型的区别  

  | Key-value数据库                                              | 关系数据库                                                   |
  | ------------------------------------------------------------ | ------------------------------------------------------------ |
  | * 数据通过键来识别，数据的值是可以动态变化的。  <br />* 有的实现中，值都是字符串类型的，有的实现中值能为整数，字符串和列表。  <br />* 一个域的数据没有严格的定义。 | * 数据模型是被严格定义的，表之间有约束和关系，通过约束和关系来保证数据的完整性。  <br /> * 数据模型依赖于数据之间的关系，而不是系统的功能。  <br />* 通过数据标准化来数据重复，这种标准化建立了数据表之间的关系。 |

* Redis  
  Redis 是一个Key-value类型的内存数据库，整个数据库统统加载在内存当中进行操作，定期通过异步操作把数据库数据刷新到硬盘上进行保存。Redis性能非常出色，每秒可以处理超过10万次读写操作。  
  Redis还支持保存List链表和Set集合的数据结构，而且还支持对List进行各种操作，例如从List两端push和pop数据，取List区间，排序等等，对Set支持各种集合的并集交集操作。

2 . **项目所需的API**   

* Stage1：  

  * 创建一个基于文件存储的K-V数据库，支持基本操作  

    * API1：KVDBHandler

    * API2：get/set/del  

  * 使用Append-Only File 存储 Key-Value数据  
    * API3：purge    
  * 处理异常情况
    * Return CODE  

* Stage2：  
  *   通过内存索引提升查找速度  
      * Append-Only File 的 Key-Offset 索引  
  *   支持过期删除操作  
      * API4：expires  

***

### 二 .设计与实现   

#### FileBase    

* API1：打开，关闭数据库（文件）  

  在这里，为KVDB数据库创建一个句柄类，方便后面操作。  

  ```c++
  //File Handler for KV-DB
  class KVDBHandler
  {
      private:
      ofstream out;
      ifstream in;
      public:
      //Constructor,create DB handler
      KVDBHandler(const std::string &db_file);
      //Closes DB handler
      ~KVDBHandler();
  }
  ```

  在这里，我为这个类添加了两个成员分别是用于写文件和读文件的文件流，构造函数则是用来根据参数来创建一个数据库。  

  ```c++
  KVDBHandler::KVDBHandler(const string &db_fie)
  {
      out.open(db_file,ios::out|ios::app|ios::binary);
      if(out.is_open())
      {
          //进行想要的操作
      }
   out.close();
  }
  KVDBHandler::~KVDBHandler()
  {
      if(out.is_open())
          out.close();
      if(in.is_open())
          in.close();
  }
  ```

  在这里通过open函数即可打开以db_file为名的数据库，而后面的参数代表该数据库支持写，末尾添加和以二进制的形式写入。  
  **注意：**  
  1.若文件存在，则打开数据库；否则创建新的数据库。  
  而其实open函数已经就可以做到这点，不需要再多加判断。  
  2.处理异常：  
  判断文件是否正常创建。  
  这里我用了一个 `is_open()`函数来判断文件是否已经正常打开。  

  

* API2：数据库基本操作：  
  这一部分的内容就比较重要了。关系着下面内容的每一步操作，缺一不可。  

  在这里我们使用的是以Append-Only File 存储 Key-Value 数据。  

  * 动机：  
    * 传统磁盘的顺序读，写性能远超过随机读，写； 
    * 不需要管理文件“空洞”    

  ![图1](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102112555381.png)

  ​									图1  (AppendOnlyFile)  
  Append Only File 的形式如图1所示  ,可以看到每一个Key-Value的record是由存储key长度的4个字节，存储value长度的4个字节，以key的长度为字节数存储key和以value长度为字节数存储value组成。并且每一个新来的k-v都会添加在末尾。  

  我们的数据库基本操作有Set，Get，Del：  

  ```c++
  int set(KVDBHandler *handler,const std:string &key,const std:string &value);  
  int get(KVDBHandler *handler,const std:string &key,std:string &value);
  int del(KVDBHandler *handler,const std:string &key);
  ```

  1.写（Set）操作  

  * 将新的Key-Value（K-V Record）追加写入（Append）在文件末尾    
  * K-V Record 由四个项构成：定长4字节存储Key的长度；定长4字节存储Value的长度；变长存储Key；变长存储Value；  

  ```c++
  out.open(filename, ios::out | ios::binary | ios::app);  
  dlen d1(key.length(), value.length());
  figure d2(key, value);
  if (out.is_open())
     setdata(d1, d2, out);
  out.close();
  ```

  在这里，我用`open()`函数打开数据库，并且用了两个结构体 dlen 和figure 封装key的长度，value的长度，key和value的值。然后通过我自定义的setdata函数把一个K-V record 写入数据库里。  

  setdata函数初步如下：  

  ```c++
  void setdata(dlen& d1, figure& d2, ofstream& out)
  {
      out.write((char*)&d1.klen, 4);
      out.write((char*)&d1.vlen, 4);
      out.write((char*)&d2.key[0], d1.klen);
      out.write((char*)&d2.value[0], d1.vlen);
  }
  ```

  通过文件流的`write()`函数即可写入文件里。

  2.删除（Del）操作：

  * 在文件末尾追加写入一个特殊的K-V Record，标记为删除。

    例如，写入一个Value的长度为-1的特殊K-V Record，表示该Key被删除

    ![图2](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102115145727.png)

    ​										图2  

    ```c++
    void KVDBHandler::del(const string& key)
    {
      	dlen d1(key.length(), -1);
        figure d2(key, "");
        out.open(filename, ios::out | ios::binary | ios::app);
        if (out.is_open())
            setdata(d1, d2, out);
        out.close();
    }
    ```

    在这里，其实删除也比较简单，只需在file里追加即可，注意的是value的长度为-1代表该key被删除，就不需要存储value了。即删除的key-value record由定长4字节存储key的长度，定长4字节存储value的长度（-1），和变长字节存储key三个部分组成。  
    这样我们就需要修改一下setdata函数了：  

    ```c++
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
    ```

  3.读（Get）操作：  

  * 顺序遍历文件，比较每个K-V Record 的Key，获取满足查询条件的最后一个Key，并返回其Value；或返回空；  
  * 注意，要处理表示删除操作的K-V Record；   

  ```c++
  int KVDBHandler::get(const string& key,string& value)
  {
      in.open(filename,ios::in);
      if(in.is_open())
      {
          while(in.peek()!=EOF)
          {
              int kl,vl;
              in.read((char *)&kl,4);
              in.read((char *)&vl,4);
              string t1;
              in.read((char *)&t1[0],kl);
              if(t1==key)
              {
                  if(vl!=-1)
                      in.read((char *)&value[0],vl);
                  else
                      value="";
              }
              else
              {
                  if(vl!=-1)
                      in.seekg(vl,ios::cur);
              }
          }
      }
  }
  ```

  在这里，get函数就是读取数据库里的每一个key-value record ，判断一下key值是否相等，若相等还需要进一步判断该key是否已被删除（根据value的长度是否为-1判断），如果key值相等并且没有被删除，则读取value，然后继续读取（因为要取出满足查询条件的最后一个key），若不相等则比较一下该key是否被删除，没有删除则用`seekg()`函数让读指针跳过读取下一个。如此反复直到末尾。  

  * 在这里，讲一下`seekg()`函数的用法  
    `seekg()`是对输入文件定位，有两个参数：第一个是偏移量，第二个则是基地址（即从哪里开始偏移）。
    对于第一个参数，可以是正负数值，正的表示向后偏移，负的表示向前偏移。而第二个参数可以是：
      ios：：beg：表示输入流的开始位置
      ios：：cur：表示输入流的当前位置
      ios：：end：表示输入流的结束位置  

  * `peek()`函数的用法  
    peek 成员函数与 get 类似，但有一个重要的区别，当 get 函数被调用时，它将返回输入流中可用的下一个字符，并从流中移除该字符；但是，peek 函数返回下一个可用字符的副本，而不从流中移除它。

* API3：清理Append-Only File  

  * 处理文件膨胀问题：

    * 背景：当Set/Get/Del操作反复执行后，文件体积会越来越大，但其中有效数据可能很少；  
      例如：  
      ![image-20210102142442558](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102142442558.png)

      ​							图3  
      可以看到图3 当我们进行了很多的Set/Get/Del操作之后，其实有效的数据很少，文件里前部分的大多是冗余的。

    * 处理方案：  
      增加purge（）操作，整理文件：  

      * 合并多次写入（Set）的Key，只保留最终有效的一项。  
      * 合并过程中，移除被删除（Del）的Key

    * 设计：  
      我的设计是用STL里的map模块实现，主要利用了它的一一映射的好处，因为要清理文件，所以我想先用map把有效的key-value存进map里保存，再把原文件的数据全部删除，最后通过map在把有效的数据写进原文件，这样就可以达到初步的清理文件效果。  

      ```c++
      string readdata(ifstream& in, int len)
      {
          char* tv = new char[len + 1];
          in.read(tv, len);
          tv[len] = '\0';
          string t2 = string(tv);
          return t2;
      }
      KVDBHandler::KVDBHandler(const string &db_file)
      {
          map<string,string> mapDB;
       	map<string,string> ::iterator it;   //迭代器   
          in.open(filename,ios::out|ios::app|ios::binary);
          if(in.is_open())
          {
              while(in.peek()!=EOF)
              {
                  int kl,vl;
                  in.read((char *)&kl,4);
                  in.read((char *)&vl,4);
                  string tkey=readdata(in,kl);
                  it=mapDB.find(tkey);
                  if(it!=mapDB.end())
                  {
                      if(vl!=-1)
                          mapDB[tkey]=readdata(in,vl);
                      else
                          mapDB.erase(it);
                  }
                  if(vl!=-1)
                  {
                    
                    string tvalue=readdata(in,vl);  mapDB.insert(pair<string,string>(tkey,tvalue));
                  }
              }
          }
          in.close();
      }
      void KVDBHandler::purge()
      {
          remove(filename.c_str());
       	out.open("temp.txt",ios::out|ios::binart|ios::app);
          map<string,string>::iterator it;
         for(it=mapDB.begin();it!=mapDB.end();it++)
         {
             set(it->first,it->second);
         }
         out.close(); rename("temp.txt",filename.c_str());
      }
      ```

      可以看到，我先在构造函数里读取文件，把key-value存进map里，为什么放在构造函数里？因为如果放在purge函数里就需要每一次的遍历文件，太耗时间，放在构造函数里的好处就是只需遍历一次，并且在以后的set/del操作里只需把对应的key增加或删除进map即可，比较方便。  

      在purge函数里，先用`remove()`函数删除原文件，然后用一个temp文件把map里的有效数据存进temp文件里，最后只需用`rename()`函数重命名为原文件即可。 这样整个purge函数已初步完成。

  * 处理异常情况：  

    * 每个API都需要处理由输入错误，系统错误等导致的异常，例如：  

      * KVDBHandler::KVDBHandler(db_file) - 若 db_file 路径不存在，需要返回 KVDB_INVALID_AOF_PATH;

      * set(handler, key, value) 或 get(handler, key) - 若 key ⻓度为0，需返回 KVDB_INVALID_KEY；

        例如：  在set函数和get函数里加上key长度的判断。

        ```c++
           if (key.length() == 0)
            {
               return KVDB_INVALID_KEY;
            }
        ```

  到这里Stage1 的内容已经完成了，那么在stage1里我们已经能够形成一个能存储，查询，删除数据的Key-Value数据库，并且学会了基本的软件工程方法。  



#### Index(内存索引)

* 动机：  
  读（GET）操作需要扫描整个Append-Only File，效率较低。  

* 解决方案：  

  增加一个索引（Index），保存当前数据库中每一个Key，在Append-Only File中的位置（Offset）  

* 建立索引：    
  遍历 Append-Only File 中的 K-V Record，在索引中保存读取的 Key 及 Record 的位置（Offset）  

1.**建立索引：**  
在这一部分建立索引我采用了hash表来实现，因为hash表的查找速度和效率要比map还要高。那么为了建立hash表，首先我需要写一个hash表。  

* 先建立一个结构体代表hash表中的一个结点：  

  ```c++
  struct hashnode
  {
      string key;
      int offset;
      hashnode* next = NULL;
  };
  ```

  这里一个节点由key，偏移量（offset）组成。

* 然后创建hash表：  

  ```c++
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
      void hash_insert(const string& key, const int &offset);
      void hash_delete(const string& key);
      void hash_set(const string& key, const int& offset)
          int hash_find(const string& key);
  };
  ```

  可以看到，我是用链地址法创建哈希表的，首先是初始化每个头节点为空，然后写了一个哈希函数（除留余数法），还有一些插入（hash_insert），删除（hash_delete）,等基本方法。后面还会有补充的。  

有了哈希表之后，就可以建立key与offset一一映射的关系了，这里需要摒弃之前map的用法，改用自己所写的哈希表：  

```c++
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
```

这段代码是加在构造函数里的，原因上面已经讲过了，可以看到和之前写map时的做法基本一致，只不过要注意，把偏移量和key插入到哈希表里之后，要记得让读指针往后移动value的长度个字节，否则会发生严重的错误。其中，里面的`hash_find()`函数是用来判断key值是否存在于哈希表里。这些hash表里的具体函数就不贴代码了，详细代码可以看附件代码。

2.**使用索引 ：** 

建立完索引之后当然可以使用索引了。  

* 读（GET）操作只需要访问索引（Index）：  
  * 若 Key 在索引中，则从索引指向的 Append-Only File 中对应的 K-V Record 中读取数据；
  * 若索引中 Key 不存在，则直接返回结果
* 读操作的时间复杂度从“O（N）”降低到“O（1）”

```C
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
        return KVDB_OK;
    }
}
```

可以看到我的get函数最终就可以变成这样了，通过哈希表能够迅速高效率的找到key值所对应的value。  

首先是判断一下key是否在哈希表里，若不在则value为空，否则就可直接通过偏移量找到原文件里的value值了。

3.**维护索引：**  

* 写入（SET）操作中，先按原方案将K-V Record 写入 Append-Only File中，再修改Index：

  * 若Key不存在，则将它添加到索引中；若Key之前已存在于索引中，则修改索引指向的位置（Offset）；  

  ```c++
  int KVDBHandler::set(const string& key, const string& value)
  {
      if (key.length() == 0)
      {
          return KVDB_INVALID_KEY;
      }
      else
      {
          out.open(filename, ios::out | ios::binary | ios::app);
          dlen d1(key.length(), value.length());
          figure d2(key, value);
          if (out.is_open())
              setdata(d1, d2, out);
          out.close();
          in.open(filename, ios::in);
          int off = getoffset(in, key);
          in.close();
          if(off!=-1)
              table->hash_insert(key,off);
  }
  ```

  这段代码就是先按原方案把key-value写入原文件里，然后通过`getoffset()`函数获取偏移量，再通过`hash_insert()`函数插入哈希表里，在插入时会判断key是否已经存在哈希表里，存在则覆盖掉之前的偏移量，不存在则插入新的即可。  
  `getoffset()`函数：  

  ```c++
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
  ```

* 删除（DEL）操作中，先按原⽅案将表示删除操作的特殊的 K-V Record 写⼊ Append-Only File 中，再将索引中的 Key 删除    
  删除操作的维护与Set函数非常类似，因为del可以说是另类的set，基本与set类似。  

  ```c++
  int tt = table->hash_find(key);
      if (tt != -1)
          table->hash_delete(key);
  ```

  这段代码只需放在DEL函数后面即可，前面的写入操作不需要修改，首先是通过`hash_find()`函数判断key是否在哈希表里，若在则`hash_delete()`删除即可，不在则不做操作。

* 清理（PURGE）Append-Only File后，需要重新执行建立索引操作。  

  维护Purge操作则比较难。  

  ```c++
  void KVDBHandler::purge()
  {
      ofstream temp;
      temp.open("temp.txt", ios::out | ios::binary | ios::app);
      in.open(filename, ios::in);
      table->hash_writefile(in,temp);
      in.close();
      temp.close();
      remove(filename.c_str());      rename("temp.txt",filename.c_str())
      ifstream tempin;
      tempin.open(filename, ios::in);
      table->hash_change_offset(tempin);
      tempin.close();
  }
  ```

  在这里可以看到，首先创建一个中转文件（temp.txt）用于把有效的数据存进去，通过调用`hash->wirtefile()`函数把有效数据写进去。然后同样是remove删除原文件，再rename重命名就可让有效数据存到原文件了。最后不要忘了要把哈希表的偏移量更新，因为数据经过清理后偏移量会改变，所以我通过`hash_change_offset()`函数来更新偏移量。  

  ```c++
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
  ```

  这是`hash_writefile()`的函数，一个参数是读文件，另一个是写文件，目的是把读文件里的有效数据读出来写进写文件里。首先遍历哈希表，获得key的偏移量然后在读文件里获取对应的value，然后再通过setdata函数写入写文件里。  

  ```c++
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
  ```

  这个是更新哈希表里偏移量的函数，通过遍历文件，`hash_set()`重新覆盖之前的偏移量就可完成更新。  



#### 过期删除操作  

* API4 定时删除：

  * 过期（EXPIRES）操作，设置Key的生存周期（秒），倒计时归零后，自动将Key删除  

  **1.**在 **Append-Only File** 中增加 **K-V Record**，记录 **Key** 的过期时间

  **1) K-V Record** 增加操作类型字段：**SET / DEL / EXPIRES** 

  **2.** 使⽤ 最⼩堆（**Min-Heap**）记录所有 **Key** 的过期时间：

  **1)** 读（**GET**）操作前，遍历堆顶（**Top**）元素，将所有已过期的 **Key** 删除（**DEL**）；

  **2)** 对重复设置过期时间的 **Key**，需更新 最⼩堆（**Min-Heap**）中的过期时间 ；

  **3)** 对已设置过期时间的 **Key** ，过期前执⾏删除（**DEL**）操作，或覆盖（**SET**）操作，需删除最⼩堆（**Min-Heap**）中过期时间；  

* 设计与实现  
  为了实现过期删除操作，首先需要一个map来记录其key和将要失效的时间，并且还需要记录设置key的生存周期时间时的时间，以便判断什么时候失效。有了map还不够，还需要一个能够根据各个key值失效时间的大小进行排序的数据结构，因为是要生存周期越小的key先删除。这里就要用到最小堆即Min-Heap。  

  1.mapTime的创建 ：  
  这里我需要一个map来记录key和其生存周期和设置生存周期时的时间，因为map只能一一映射，为了实现一对多，我采用了vector存储key的生存周期和设置生存周期时的时间。因此  

  ```c++
  map<string, vector<int>>mapTime;
  ```

  可以创建一个一对多的map了。   


   首先需要用另一个文件专门存储key值，生存周期和设置生存周期时的时间，用于在每次打开原文件时都能立刻获取到mapTime的内容，防止丢失数据。  

  ```c++
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
  ```

  这段代码是在构造函数里的，其实就是跟之前的map操作基本一致，这里我令如果key没有设置生存周期的话就让其为-1代表没有。同样是读取文件，若生存周期不为-1，则用一个vector存其周期和设置周期时的时间，然后判断一下是否找得到key值，找不到则插入map里，否则覆盖掉对应map的值。  

  2.Min-Heap的创建：  
  最小堆其实就是个优先队列，首先需要用一个结构体当作一个节点用来存储key，生存周期，设置生存周期时的时间。  

  ```c++
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
  ```

  这里记得要重载大于号 ，因为我是根据生存周期来比较大小的。  

  ```c++
  priority_queue<Node, vector<Node>, greater<Node>> pq;
  ```

  然后就可创建出最小堆了。  

  初始化最小堆，目的也是防止数据丢失，最小堆根据mapTime来初始化。  

  ```c++
      map<string, vector<int>>::iterator it;
      for (it = mapTime.begin(); it != mapTime.end(); it++)
      {
          Node temp(it->first, it->second[0], it->second[1]);
          pq.push(temp);
      }
  ```

  这里只需遍历mapTime即可一个一个加入Min-Heap完成初始化。  

  3.应用map和Min-Heap：  

  * 首先是为key设置生存周期时的操作：  

    ```c++
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
    ```

    通过expires函数来设置，参数是key和生存周期。首先需要往mapTime里添加，如果不存在就插入，存在就覆盖掉之前的时间，然后还需往min-heap里插入相应的节点，最后还需往记录节点的文件里添加节点防止数据丢失。  （这里的`time(NULL)`是c++里可以获取电脑当前的时间，并且是以秒数的形式表达的）。  

  * 读操作前，需要遍历堆顶元素，将所有已过期的Key删除。  
    这个比较简单，就像队列一样，一个个取出堆顶元素，判断一下当前的时间Key是否已经过期，如果过期了就要同时把mapTime，最小堆，原文件和记录生存周期的文件里的Key全部删除。最后不要忘了重建最小堆，因为此时最小堆为空，所以要根据mapTime重建最小堆。  

    ```c++
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
    ```

  * 可能还会有重复设置Key的过期时间的情况，因此需要对此情况做出应对。这个已经在上面expires函数里完成了，重复设置的话就覆盖掉之前的mapTime对应值，并且插入min-heap里，还要写进记录生存周期的文件里。  

  * 当对已经设置了过期时间的Key，过期前删除或覆盖的话，需要删除最小堆里的过期时间。  
    为此我需要在Del函数里修改一下，加入一下代码：  

    ```c++
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
    ```

    在这里，我是在mapTime里进行了删除和在记录生存周期的文件里删除了对应的Key（以生存周期为-1代表删除），并不需要在最小堆里删除，因为最小堆的删除操作比较困难，只需删除上面两个即可，因为当get的时候，我是需要将堆顶元素的过期时间与mapTime里对应的过期时间对比的，若不相等则不删除（因其不存在），若相等才代表删除。  

    对于Set操作，同样在后面加上跟Del后面一样的代码，因为都是代表删除最小堆，意思是Key值已经不存在了，所以生存周期自然也没有了。  


到这里，Stage2的内容已经完成了。在Stage2里  

* 优化读取效率的Key-Value 数据库  
* 使用HashMap实现索引，支持内存索引的重建，维护操作  
* 实现Min-Heap，支持计时器的重建，维护操作 



#### 日志库  

日志库其实就是方便自己调试，查看数据之间的变化而设置的，在这里我写了一个简单日志库可供自己方便查看提示。  
我用一个Logger类来实现：  

* 日志等级：我设置了5个日志等级，从低到高依次为**debug**，**info**，**warning**，**error**，**fatal**，日志等级简单来说就是当日志等级大于等于设置的初始等级时才会记录日志。  
* 输出目标：我设置了三种输出目标：仅输出到终端，仅输出到文件，既输出到终端又输出到文件  

为了方便设置日志等级，可以用一个枚举类表示五种日志等级，同理用一个枚举类表示三种输出目标  

1. ```c++
   enum log_level { debug, info, warning, error, fatal };
   ```

2. ```c++
   enum log_target { file, terminal, file_and_terminal };
   ```

为了更好地管理日志，我用一个类封装了日志工具，在构造函数中设定日志等级、输出目标、日志文件路径等，并提供DEBUG、INFO、WARNING、ERROR、FATAL五个接口
对于日志内容还增加了一些前缀，比如使用__FILE__宏表示当前运行位置所在的文件，封装一个`getcurtime()`函数用以记录日志记录时间等  

```c++
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
```

```c++
size_t strftime(char *strDest, size_t maxsize, const char *format, const struct tm *timeptr);
```

根据格式字符串生成字符串。  

```c++
struct tm *localtime(const time_t *timer);
```

取得当地时间，localtime获取的结果由结构tm返回

其实获取当前时间还可以这样  

```c++
time_t now_time;
now_time = time(NULL);
```

只不过这样的时间是秒数，需要通过相应的时间换算。

类的成员和函数如下：

```c++
    enum log_level { debug, info, warning, error, fatal };
    enum log_target { file, terminal, file_and_terminal };
    ofstream out;//将日志输出到文件的流对象
    log_target target;//日志输出目标
    log_level level;//日志等级
    string path; //日志文件路径
void display(string text, log_level reallevel);
    Logger();
    Logger(log_target real_target, log_level real_level, string tpath);
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
```

`display()`函数用来输出日志信息的

```c++
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
```

当前等级设定的等级才会显示在终端，且不能是只文件模式  若是文件，则写入文件里。  
日志库的实现到这里已经具有基本功能，可以在想要输出信息的位置添加`logger.debug("something...")`形如这样的代码即可。



### 三.测试  

有了之前的代码，我们需要测试一下是否符合要求，是否出现错误。  

#### Stage1的测试  

* 对Set/Get/Del的测试：  

  ![图4](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223111958.png)

  ​							图4  

  ![图5](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223155589.png)

  ​								图5  
  可以看到我先创建一个以“example”为名的txt文件，并对其set（a,123）,set(b,234)  

  ![图6](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223322450.png)

  ​									图6  
  可以看到成功写入，再试一下Get操作，get（a）得到如下：  
  ![图7](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223445233.png)

  ​									图7

  同样成功，再试一下del操作，  
  ![图8](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223538936.png)

  ​								图8  
  可以看到，这里我先Delete（b），然后再get（b），得到的结果为空，说明删除成功。  
  ![图9](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102223650334.png)

  ​								图9  

* 对Purge操作进行测试  ：  

  为了确保没有偶然性，我再进行了set(c,4325),set(c,678),set(e,64565),del(a),del(c),purge()这一系列操作之后，得到的结果也正确  ![图10](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102224026806.png)

  ​									图10  

#### Stage2的测试   

* 过期删除操作：  

  * 先是expires（e,5）,让e5秒后失效，

    ![图11](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102225315299.png)

    ​							图11  
    可以看到，我在5秒内连输入两次get（e），都能获得对应的value ，在5秒之后就取不到了。  

  * 重复设置Key的过期时间  
    ![图12](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102225945004.png)

    ​									图12

    可以看到，我先是让c5秒后过期，再重复设置为10秒后过期，在5秒时get（c），可以得到value说明没有被删除，在10秒后get（c），c已被删除。重复设置的操作成功，没有出现问题。

  * 在过期之间执行Del/Set操作  
    ![图13](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102230357014.png)

    ​								图13  
    可以看到，我先是让d在10秒之后过期，然后我在10秒内del（d），在10秒之后，get（d）得到的值为空。测试正确。  

    ![图14](C:\Users\XIE\AppData\Roaming\Typora\typora-user-images\image-20210102230715895.png)

    ​									图14  
    这里我让d在10秒之后过期，然后在10秒内set（d，543）重新覆盖掉d的值，在10秒之后仍能取得d的value值，故测试成功。

### 四.出现的问题  

1.出现内存泄露  ：  
可能的原因是每次文件打开之后没有关闭，可能会导致不可预料的错误。  

2.读取文件时发生错误，读不到值  
原因是根据偏移量移动到key后面时读完之后没有让读指针往下移动，这点需要注意。  

3.在get操作之前要更新最小堆的内容，但是别忘了在之后需要重建最小堆，因为此时最小堆为空。

4.在过期操作里，发现过了指定的时间之后并没有删除，而是要更久的时间才能删除。  
原因是我没有把设置key的生存周期时的时间记录下来，导致时间会变化，最后我的map就改为一对多，用vector把生存周期，和设置生存周期时的时间一并记录下来，这样就确保时间不会发生变化了。



### 五.心得与体会

一开始做这个项目感觉很难什么也不会，但经历了这一学期之后的学习与实践，感觉其实也并不是很难，只要肯花功夫。才刚开始的无从下手各种找资料，到后来开始理解了思路，实现各个部分的功能，调试成功的那一刻，都让我感到一点成就感。并且能够学习到课本之外的东西，理解了做项目的一些流程和所需的工具，这或许能为我以后积累一点经验。能够学会自己发现问题然后查找资料解决问题。而且在做项目过程中与同学们的讨论也让我学到了不同的思路，不同的见解，通过交流让我的程序更完善。总的来看，这个项目不大但也需要付出很多，在这个过程中能让学习到很多知识，学会一些工具的使用。当然我做得也还有很多不足，相信以后能够改正过来，更好的学习，提升自己。



### 六.参考文献

> 肖红玉, 陈海, 黄静. Key-Value数据库的研究与应用[J]. 自动化与仪器仪表, 2010(04):87-89.

> [peek函数的用法]（http://c.biancheng.net/view/1536.html）  

> [seekg函数详解]（https://blog.csdn.net/qq_26822029/article/details/51790468）

> [map函数的用法]（https://blog.csdn.net/weixin_43828245/article/details/90214494）

> [实现日志类]（https://blog.csdn.net/baidu_41743195/article/details/107359665?utm_medium=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.pc_relevant_is_cache&depth_1-utm_source=distribute.pc_relevant.none-task-blog-BlogCommendFromMachineLearnPai2-1.pc_relevant_is_cache）

> [优先队列的重载]（https://blog.csdn.net/sinat_37205608/article/details/82460301）

> [获取系统时间的方法]（https://www.cnblogs.com/zwind/p/4165119.html)

