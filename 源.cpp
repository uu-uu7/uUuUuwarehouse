#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <windows.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#define access _access
#define MKDIR(dir) _mkdir(dir)
#define PATH_SEP '\\'
#ifdef _MSC_VER
#define strdup _strdup
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & S_IFMT) == S_IFREG)
#endif
#endif
#else
#include <unistd.h>
#define MKDIR(dir) mkdir(dir, 0777)
#define PATH_SEP '/'
#endif
#include <sys/types.h>
#include <sys/stat.h>
#define IO_BUF_SIZE 1048576
long long get_file_size(const char* filename) {
	struct _stat64 st; 
	if (_stat64(filename, &st) == 0) {
		return st.st_size;
	}
	return -1;
}
typedef struct huffman_node {//哈夫曼树节点
	long long num;
	unsigned char name;
	struct huffman_node* left;
	struct huffman_node* right;
}node;
typedef struct file_info {//文件信息结构体
	char name[256];//完整路径
	char name1[512];//相对路径
	long long size;
	int in;
}fio;
typedef struct frequency {//频率结构体
	long long num;
	char name;
	int in;//是否存在
	node* point;//指向哈夫曼树节点
}fre;
typedef struct Code {//编码结构体
	unsigned long long code_number;
	int code_length;
}code;
void free_tree(node* root) {//释放哈夫曼树内存
	if (root == NULL) {
		return;
	}
	free_tree(root->left);
	free_tree(root->right);
	free(root);
}
int comp(const void* a, const void* b) {//比较函数
	fre* va = (fre*)a;
	fre* vb = (fre*)b;
	if (va->num < vb->num) return -1; 
	if (va->num > vb->num) return 1;  
	if (va->name < vb->name) return -1;
	if (va->name > vb->name) return 1;
	return 0;
}
int check_path_type(const char* path) {
	DWORD attr = GetFileAttributesA(path);
	if (attr == INVALID_FILE_ATTRIBUTES) {
		return 0;
	}
	if (attr & FILE_ATTRIBUTE_DIRECTORY) return 1; // 文件夹
	return 2; // 普通文件
}

node* create_node() {//创建哈夫曼树节点
	node* node1 = (node*)malloc(sizeof(node));
	node1->num = 0;
	node1->name = 0;
	node1->left = NULL;
	node1->right = NULL;
	return node1;
}
fre findmin(fre* num, fre* num1, int*a, int*b) {//寻找最小值a和b分别是num和num1的当前位置的下标
	fre min;
	if (num1[*b].in != 0&&*a<256) {
		if(num[*a].num < num1[*b].num){
			min=num[*a];
			(*a)++;
		}
		else{
			min=num1[*b];
			(*b)++;
		}
	}
	else {
		if(*a>=256){
			min=num1[*b];
			(*b)++;
		}
		else
		min = num[*a];
		(*a)++;
	}
	return min;
}
void quote_delete(char* filename) {//删除路径中的引号
	size_t len = strlen(filename);
	if (len >= 2 && filename[0] == '"' && filename[len - 1] == '"') {
		memmove(filename, filename + 1, len - 2);
		filename[len - 2] = '\0';
	}
}
void frequency(const char* filename, fre* num,int model,fio*file_info,int*file_count) {//统计频率
	if (*file_count >= 1000) {
		printf("警告：文件数量已达到上限 %d，停止统计！\n", 1000);
		return;
	}
	if (model == 2) {//文件模式
		FILE* fp = fopen(filename, "rb");
		char buffer[2048] = { 0 };
		int buffer_size = 0;
		if (fp) {
			while ((buffer_size = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
				for (int i = 0; i < buffer_size; i++) {
					unsigned char ch = buffer[i];
					num[ch].num++;
					num[ch].name = ch;
				}
			}
			fclose(fp);
			file_info[*file_count].size = get_file_size(filename);//记录文件大小
			(*file_count)++; 
		}
		else {
			printf("文件打开失败");
		}
	}
	else {
		if(model==1){//文件夹模式
			char path_dir[256] = "";
			snprintf(path_dir, sizeof(path_dir), "%s\\*", filename);
			struct _finddata_t file;
			intptr_t handle = _findfirst(path_dir, &file);
			if(handle!=-1){
				do{
					if (strcmp(file.name, ".") == 0 || strcmp(file.name, "..") == 0) {//忽略当前目录和父目录
						continue;
					}
					if((file.attrib & _A_SUBDIR) == 0){//记录文件信息
						char full_path[512];
						snprintf(file_info[*file_count].name1, sizeof(file_info[*file_count].name1), "%s", file.name);//相对路径
						snprintf(full_path, sizeof(full_path), "%s\\%s", filename, file.name);//完整路径
						snprintf(file_info[*file_count].name, sizeof(file_info[*file_count].name), "%s", full_path);//完整路径
						frequency(full_path, num,  2, file_info,file_count);//统计频率
					}
					else {
							char sub_dir_path[512];
							snprintf(sub_dir_path, sizeof(sub_dir_path), "%s\\%s", filename, file.name);
							frequency(sub_dir_path, num, 1, file_info, file_count);
					}
				}while(_findnext(handle, &file) == 0);
				_findclose(handle);
			}
			else{
				printf("目录打开失败");
			}
		}
	}
}
void generate_newname(const char* name, char* newname, int model, char* filesuffix) {//命名新文件
	char path_dir[256] = "";//备份路径
	char prefix[256] =""; // 用于存储主文件名
	char suffix[256]=""; // 用于存储文件后缀
	const char* last_slash = strrchr(name, '\\'); // 找 Windows 的反斜杠
	if (last_slash == NULL) {
		last_slash = strrchr(name, '/');          // 兼容 Linux/Mac 的正斜杠
	}
	const char* pure_name; // 指向纯文件名的指针
	if (last_slash != NULL) {//定位路径和纯文件名
		int path_len = last_slash - name + 1;
		strncpy(path_dir,name, path_len); 
		path_dir[path_len] = '\0';
		pure_name = last_slash + 1;        
	}
	else {
		pure_name = name;
	}
	if (model == 1) {//压缩模式
		const char* dot = strrchr(pure_name, '.');
		if (dot != NULL) {
			int len = dot - pure_name;
			strncpy(prefix, pure_name, len);
			prefix[len] = '\0';
			strcpy(filesuffix, dot);
		}
		else {
			strcpy(prefix, pure_name);
		}
		strcpy(suffix, "zip");
		}
	else {//解压缩模式
		int len = strlen(pure_name);//检查是否以.zip结尾，如果是则去掉后缀
			if (len > 4 && _strnicmp(pure_name + len - 4, ".zip", 4) == 0) {
				strncpy(prefix, pure_name, len - 4);
				prefix[len - 4] = '\0';
				suffix[0] = '\0'; // 新文件无后缀
			}
			else {
				strcpy(prefix, pure_name);
				suffix[0] = '\0';
			}
		}
	if (suffix[0]) {//有后缀
		sprintf(newname, "%s%s.%s", path_dir, prefix, suffix);
	}
	else {//没有后缀
		sprintf(newname, "%s%s", path_dir, prefix);
	}
	if (access(newname, 0) == 0) {//检查文件是否存在，存在则重命名
		int i = 1;
		while (1) {
			if (suffix[0]) {//有后缀
				sprintf(newname, "%s%s(%d).%s", path_dir, prefix, i, suffix);
			}
			else {//没有后缀
				sprintf(newname, "%s%s(%d)", path_dir, prefix, i);
			}
			if (access(newname, 0) == -1) {
				break;
			}
			i++;
		}
	}
}
void generate_code(node* root, code* codes, unsigned int current_code, int current_length) {//生成编码
	if (root->left == NULL && root->right == NULL) {
		codes[(unsigned char)root->name].code_number = current_code;
		codes[(unsigned char)root->name].code_length = current_length;
		return;
	}
	if (root->left != NULL) {
		generate_code(root->left, codes, (current_code << 1), current_length + 1);
	}
	if (root->right != NULL) {
		generate_code(root->right, codes, (current_code << 1) | 1, current_length + 1);
	}
}
int encode(code* Code, FILE* fp1, char* file) {//编码
	FILE* fp = fopen(file, "rb");
	if (fp == NULL) {
		printf("错误：无法打开源文件 %s\n", file);
		return -1;
	}
	unsigned char* buffer = (unsigned char*)malloc(IO_BUF_SIZE);//读取缓冲区
	unsigned char* write_buf = (unsigned char*)malloc(IO_BUF_SIZE);//写入缓冲区
	if (!buffer || !write_buf) {//判断是否成功创建
		free(buffer);
		free(write_buf);
		fclose(fp);
		printf("错误：内存分配失败\n");
		return -1;
	}
	size_t write_pos = 0;//记录写入进度
	size_t buffer_size = 0;//记录读取进度
	int bit_length = 0;
	unsigned int bit_buffer = 0;
	while ((buffer_size = fread(buffer, 1, IO_BUF_SIZE, fp)) > 0) {
		for (size_t i = 0; i < buffer_size; i++) {
			unsigned char ch = buffer[i];
			code code1 = Code[ch];
			unsigned int code_number = code1.code_number;
			int code_length = code1.code_length;
			for (int j = code_length - 1; j >= 0; j--) {
				int bit = (code_number >> j) & 1;
				bit_buffer = (bit_buffer << 1) | bit;
				bit_length++;
				if (bit_length == 8) {//凑满写入
					write_buf[write_pos++] = (unsigned char)bit_buffer;
					if (write_pos >= IO_BUF_SIZE) {
						fwrite(write_buf, 1, write_pos, fp1);
						write_pos = 0;
					}
					bit_length = 0;
					bit_buffer = 0;
				}
			}
		}
	}
	if (bit_length > 0) {
		bit_buffer <<= (8 - bit_length);
		write_buf[write_pos++] = (unsigned char)bit_buffer;
	}
	if (write_pos > 0) {
		fwrite(write_buf, 1, write_pos, fp1);
	}
	free(buffer);
	free(write_buf);
	fclose(fp);
	return 0;
}
int decodes(node* root, FILE* fp, char* newfile, long long length) {//编码
	if (root == NULL || length <= 0) return 0;
	FILE* fp1 = fopen(newfile, "wb");
	if (fp1 == NULL) {
		printf("错误：无法创建目标文件 %s\n", newfile);
		return -1;
	}
	unsigned char* read_buf = (unsigned char*)malloc(IO_BUF_SIZE);//创建读取缓冲区
	unsigned char* write_buf = (unsigned char*)malloc(IO_BUF_SIZE);//创建写入缓冲区
	if (!read_buf || !write_buf) {//判断是否创建成功
		free(read_buf);
		free(write_buf);
		fclose(fp1);
		printf("错误：内存分配失败\n");
		return -1;
	}
	long long count_length = 0;
	node* current_node = root;
	size_t write_pos = 0;//记录写入缓冲区进度
	size_t bytes_read = 0;//记录读入数量
	size_t read_pos = 0;//记录读入缓冲区进度
	while (count_length < length && (bytes_read = fread(read_buf, 1, IO_BUF_SIZE, fp)) > 0) {
		for (read_pos = 0; read_pos < bytes_read && count_length < length; read_pos++) {
			unsigned char bit8 = read_buf[read_pos];
			for (int i = 7; i >= 0; i--) {
				int bit = (bit8 >> i) & 1;
				current_node = (bit == 0) ? current_node->left : current_node->right;
				if (current_node == NULL) {
					free(read_buf);
					free(write_buf);
					fclose(fp1);
					printf("错误：解码失败，路径错误\n");
					return -1;
				}
				if (current_node->left == NULL && current_node->right == NULL) {
					write_buf[write_pos++] = (unsigned char)current_node->name;
					count_length++;
					current_node = root;
					if (write_pos >= IO_BUF_SIZE) {
						fwrite(write_buf, 1, write_pos, fp1);
						write_pos = 0;
					}
					if (count_length >= length) goto decode_done;
				}
			}
		}
	}
decode_done://剩余部分写入
	if (write_pos > 0) {
		fwrite(write_buf, 1, write_pos, fp1);
	}
	free(read_buf);
	free(write_buf);
	fclose(fp1);
	return 0;
}
int compress(char* filename,char*newfilename,char*filesuffix) {//压缩
	fre num[256] = { 0 };//频率储存
	fre num1[512] = { 0 };//暂时存储
	int file_count = 0;
	int model = check_path_type(filename);//检查路径类型，1为文件夹，2为文件，3为其他
	printf("[DEBUG] check_path_type(\"%s\") = %d\n", filename, model);
	fio file_info[1000] = { 0 };
	frequency(filename, num,model,file_info,&file_count);//统计频率
	qsort(num, 256, sizeof(fre), comp);//排序频率
	int head1 = 0, head2 = 0, tail = 0;
	for(head1=0;head1<256;head1++){//过滤频率为零的字符
		if(num[head1].num!=0){
			break;
		}
	}
	if(head1==256){
		printf("文件为空\n");
		return 1;
	}
	int number_of_nodes = 256 - head1;//有效字符数量
	int node_length = number_of_nodes;
	if (number_of_nodes == 1) {//只有一个字符，特殊处理
		code code[256] = { 0 }; 
		code[0].code_number = 0; 
		code[0].code_length = 1; 
		FILE* fp1 = fopen(newfilename, "wb");
		if (fp1 == NULL) {
			printf("错误：无法创建目标文件 %s\n", newfilename);
			return -1;
		}
		fwrite(&model, sizeof(int), 1, fp1);//写入压缩类型
		fwrite(&node_length, sizeof(int), 1, fp1);//写入有效字符数量
		unsigned char ch = (unsigned char)num[head1].name;
		unsigned char len = (unsigned char)code[0].code_length;
		fwrite(&ch, 1, 1, fp1);
		fwrite(&len, 1, 1, fp1);
		fwrite(&code[0].code_number, sizeof(int), 1, fp1);
		int all_filelength = strlen(filesuffix);
		fwrite(&all_filelength, sizeof(int), 1, fp1);//写入文件或文件夹后缀长度
		fwrite(filesuffix, 1, all_filelength, fp1);//写入文件或文件夹后缀
		if (model == 2) {//单文件情况单独处理，直接写入文件长度和编码内容，不需要写入文件信息
			long long all_length = get_file_size(filename);
			fwrite(&all_length, sizeof(long long), 1, fp1);//写入文件夹总长度
			encode(code, fp1, filename);
			file_count = 0;//单文件不需要写入文件信息
		}
		else {
			fwrite(&file_count, sizeof(int), 1, fp1);//写入文件数量
			for (int i = 0; i < file_count; i++) {//写入文件信息
				int filename_length = strlen(file_info[i].name1);
				fwrite(&filename_length, sizeof(int), 1, fp1);
				fwrite(file_info[i].name1, 1, filename_length, fp1);
				fwrite(&file_info[i].size, sizeof(long long), 1, fp1);
			}
			for (int i = 0; i < file_count; i++) {//依次编码每一个文件
				encode(code, fp1, file_info[i].name);
			}
		}
		fclose(fp1);
	}
	else {
		code code[256] = { 0 };//编码储存
		while (number_of_nodes > 1) {//构建哈夫曼树
			node* new_node = (node*)malloc(sizeof(node));//创建一个新的哈夫曼树节点
			fre min1, min2;
			min1 = findmin(num, num1, &head1, &head2);//寻找最小的两个节点
			min2 = findmin(num, num1, &head1, &head2);
			num1[tail].num = min1.num + min2.num;//新节点的频率为两个最小节点频率之和
			num1[tail].name = 0;
			num1[tail].in = 1;
			if (min1.in == 0) {//如果最小节点是叶子节点，创建一个新的叶子节点
				node* leaf1 = (node*)malloc(sizeof(node));
				leaf1->num = min1.num;
				leaf1->name = min1.name;
				leaf1->left = NULL;
				leaf1->right = NULL;
				min1.point = leaf1;
			}
			if (min2.in == 0) {//如果第二小节点是叶子节点，创建一个新的叶子节点
				node* leaf2 = (node*)malloc(sizeof(node));
				leaf2->num = min2.num;
				leaf2->name = min2.name;
				leaf2->left = NULL;
			    leaf2->right = NULL;
				min2.point = leaf2;
			}
			new_node->num = num1[tail].num;//进行树的连接
			new_node->left = min1.point;
			new_node->right = min2.point;
			num1[tail].point = new_node;
			tail++;
			number_of_nodes--;
		}
		generate_code(num1[tail - 1].point, code, 1, 0);
		FILE* fp1 = fopen(newfilename, "wb");
		if (fp1 == NULL) {
			printf("错误：无法创建目标文件 %s\n", newfilename);
			return -1;
		}
		fwrite(&model, sizeof(int), 1, fp1);//写入压缩类型
		fwrite(&node_length, sizeof(int), 1, fp1);//写入有效字符数量
		for (int i = 0; i < 256; i++) {//写入文件信息
			if (code[i].code_length != 0) {
				unsigned char ch = (unsigned char)i; 
				unsigned char len = (unsigned char)code[i].code_length;
				fwrite(&ch, 1, 1, fp1);
				fwrite(&len, 1, 1, fp1);
				fwrite(&code[i].code_number, sizeof(int), 1, fp1);
			}
		}
		int all_filelength = strlen(filesuffix);
		fwrite(&all_filelength, sizeof(int), 1, fp1);//写入文件或文件夹后缀长度
		fwrite(filesuffix, 1, all_filelength, fp1);//写入文件或文件夹后缀
		if (model == 2) {//单文件情况单独处理，直接写入文件长度和编码内容，不需要写入文件信息
			long long all_length = get_file_size(filename);
			fwrite(&all_length, sizeof(long long), 1, fp1);//写入文件夹总长度
			encode(code, fp1, filename);
			file_count = 0;//单文件不需要写入文件信息
		}
		fwrite(&file_count, sizeof(int), 1, fp1);//写入文件数量
		for (int i = 0; i < file_count; i++) {//写入文件信息
			int filename_length=strlen(file_info[i].name1);
			fwrite(&filename_length, sizeof(int), 1, fp1);
			fwrite(file_info[i].name1, 1, filename_length, fp1);
			fwrite(&file_info[i].size, sizeof(long long), 1, fp1);
		}
		for(int i=0;i<file_count;i++){//依次编码每一个文件
			encode(code,fp1, file_info[i].name);
		}
		if (tail > 0) {
			free_tree(num1[tail - 1].point);
		}
		printf("压缩比为：");
		if (model == 2) {//单文件情况直接计算压缩比
			printf("%lf\n", (double)get_file_size(filename) / (double)get_file_size(newfilename));
		}
		else {//文件夹情况计算压缩比
			long long total_length = 0;
			for (int i = 0; i < file_count; i++) {
				total_length += file_info[i].size;
			}
			printf("%lf\n", (double)total_length / (double)get_file_size(newfilename));
		}
		fclose(fp1);
	}
	return 0;
}
int uncompress( char* filename, char* newfilename) {//解压缩
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL) {
		printf("错误：无法打开源文件 %s\n", filename);
		return -1;
	}
	code decode[256] = { 0 };//编码储存
	int bit_all, bit_filesuffix, model;//总字符数量，文件或文件夹后缀长度，压缩类型
	char filesuffix[256] = { 0 };//文件或文件夹后缀
	char newfile[1024] = { 0 };//完整路径
	fread(&model, sizeof(int), 1, fp);//压缩类型
	fread(&bit_all, sizeof(int), 1, fp);
	for(int i=0;i<bit_all;i++){//读取码表
		unsigned char ch;
		fread(&ch,1, 1, fp);
		fread(&decode[ch].code_length,1, 1, fp);
		fread(&decode[ch].code_number,4, 1, fp);
	}
	fread(&bit_filesuffix, sizeof(int), 1, fp);//读取文件后缀长度
	int safe_len = (bit_filesuffix < 255) ? bit_filesuffix : 255;
	fread(filesuffix, 1, safe_len, fp);//读取文件后缀
	filesuffix[safe_len] = '\0';
	sprintf(newfile, "%s.%s", newfilename, filesuffix);//拼接路径
	node* root = create_node();//创建哈夫曼树根节点
	for (int i = 0; i < 256; i++) {
		if (decode[i].code_length != 0) {
			int code_length = decode[i].code_length;
			unsigned int code_number = decode[i].code_number;
			node* current_node = root;
			for (int j = code_length - 1; j >= 0; j--) {
				int bit = (code_number >> j) & 1;
				if (bit == 0) {
					if (current_node->left == NULL) {
						current_node->left = create_node();//如果左子树不存在，创建一个新的节点
					}
					current_node = current_node->left;
				}
				else {
					if(current_node->right == NULL){
						current_node->right = create_node();//如果右子树不存在，创建一个新的节点
					}
					current_node = current_node->right;
				}
			}
			current_node->name = i;
		}
	}
	if (model == 2) {//文件模式直接解码，不需要读取文件信息
		long long all_length;
		fread(&all_length, sizeof(long long), 1, fp);
		int result1=decodes(root, fp, newfile, all_length);
	}
	else {//文件夹模式需要读取文件信息
		fio file_info[1000] = { 0 };
		int file_count = 0;//文件数量
		fread(&file_count, sizeof(int), 1, fp);//读取文件数量
		if (file_count > 1000) {
			printf("错误：压缩包内文件数量异常 (%d)，超出处理上限！\n", file_count);
			fclose(fp);
			free_tree(root);
			return -1;
		}
		for (int i = 0; i < file_count; i++) {//读取相对路径和文件大小
			int filename_length;
			char NAME[256] = { 0 };
			fread(&filename_length, sizeof(int), 1, fp);//读取文件名长度
			int read_len = (filename_length < 255) ? filename_length : 255;
			fread(NAME, 1, read_len, fp);
			NAME[read_len] = '\0';
			fread(&file_info[i].size, sizeof(long long), 1, fp);
			sprintf_s(file_info[i].name1, 512, "%s\\%s", newfilename, NAME);//拼接完整路径
		}
		if (_access(newfilename, 0) != 0) {//检查文件夹是否存在，不存在则创建
			if (_mkdir(newfilename) == 0) {
				printf("成功创建文件夹: %s\n", newfilename);
			}
			else {
				printf("文件夹创建失败！\n");
				return 1;
			}
		}
		else {
			printf("文件夹已存在，无需重复创建。\n");
		}
		printf("%d\n", file_count);
		for(int i=0;i<file_count;i++){
			int deresult=decodes(root, fp, file_info[i].name1, file_info[i].size);
			if(deresult==0){
				printf("成功解压文件: %s\n", file_info[i].name1);
			}
			else{
				printf("解压文件失败: %s\n", file_info[i].name1);
			}
		}
	}
	fclose(fp);
	free_tree(root);
	return 0;
}
void copy_file_binary(const char* src_path, const char* dest_path) {
	FILE* src = fopen(src_path, "rb");
	if (!src) {
		printf("错误：无法打开源文件 %s\n", src_path);
		return;
	}
	FILE* dest = fopen(dest_path, "wb");
	if (!dest) {
		printf("错误：无法创建目标文件 %s\n", dest_path);
		fclose(src);
		return;
	}
	char buffer[4096];
	size_t bytes;
	while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		fwrite(buffer, 1, bytes, dest);
	}
	fclose(src);
	fclose(dest);
}
void create_parent_directory(const char* file_path) {
	char temp_path[512];
	strncpy(temp_path, file_path, sizeof(temp_path) - 1);
	temp_path[sizeof(temp_path) - 1] = '\0';
	char* last_sep = strrchr(temp_path, PATH_SEP);
	if (last_sep != NULL) {
		*last_sep = '\0'; // 截断，只保留目录部分
		_mkdir(temp_path);
	}
}
char* package_files(const char* packfile, int file_count) {//打包文件
	if (packfile == NULL) {
		printf("错误：打包的目标文件夹名不能为空！\n");
		return NULL;
	}
	if (MKDIR(packfile) != 0) {//创建文件
		printf("提示：文件夹 %s 可能已存在或创建失败，将继续尝试复制文件。\n", packfile);
	}
	else {
		printf("成功创建打包文件夹: %s\n", packfile);
	}
	char current_file[512] = { 0 };
	for (int i = 0; i < file_count; i++) {
		printf("请输入第 %d/%d 个文件的路径: ", i + 1, file_count);
		if (fgets(current_file, sizeof(current_file), stdin) == NULL) {
			printf("读取文件路径失败，跳过该文件。\n");
			continue;
		}
		current_file[strcspn(current_file, "\n")] = '\0';
		quote_delete(current_file);
		if (strlen(current_file) == 0) {
			printf("文件路径为空，跳过。\n");
			continue;
		}
		const char* relative_part = strrchr(current_file, PATH_SEP);
		if (relative_part != NULL) {
			relative_part++; // 跳过斜杠本身
		}
		else {
			relative_part = current_file; // 没有斜杠则为纯文件名
		}
		char dest_path[512];
		snprintf(dest_path, sizeof(dest_path), "%s%c%s", packfile, PATH_SEP, relative_part);
		create_parent_directory(dest_path);
		copy_file_binary(current_file, dest_path);
		printf("已将 %s 封装进 %s\n", relative_part, packfile);
	}
	printf("文件夹处理完成！\n");
	return strdup(packfile);
}
int main() {
    printf("=== 欢迎使用哈夫曼压缩工具 ===\n");
	printf("选择工作模式\n压缩输入：c\n解压缩输入：d\n退出输入：o\n");
	while (1) {
		char model_1;//工作模式
		int result = 1;//运行结果
		char filename[256] = { 0 };//文件名
		char filesuffix[256] = { 0 };//文件后缀
		char newfilename[512]{ 0 };//新文件名
		scanf("%c", &model_1);//读取工作模式
		int c;
		int numoffile = 0;//文件数量
		while ((c = getchar()) != '\n' && c != EOF);
		if (model_1 == 'o') {
			break;
		}
		switch (model_1) {
		case'c':printf("请输入要压缩的文件或文件夹数量:\n");
			    scanf("%d", &numoffile);
				printf("输入文件名或文件地址：\n");
				while ((c = getchar()) != '\n' && c != EOF);
				if (numoffile > 0) {
					if (numoffile == 1) {
						if (fgets(filename, sizeof(filename), stdin) != NULL) {
							filename[strcspn(filename, "\n")] = '\0';
						}
						else {
							printf("读取文件失败\n");
							continue;
						}
						quote_delete(filename);//删除首尾双引号
						generate_newname(filename, newfilename, 1, filesuffix);//命名
						result = compress(filename, newfilename, filesuffix);
					}
					else {
						printf("检测到多个文件，正在自动为您打包成一个文件夹\n");
						char packfile[256] = { 0 };
						printf("请输入打包后的文件夹名称：");
						if (fgets(packfile, sizeof(packfile), stdin) != NULL) {
							packfile[strcspn(packfile, "\n")] = '\0';
						}
						else {
							printf("读取文件夹名称失败\n");
							continue;
						}
						char* result_folder = package_files(packfile,numoffile);
						quote_delete(result_folder);
						generate_newname(result_folder, newfilename, 1, filesuffix);
						result = compress(result_folder, newfilename, filesuffix);
					}
				}
				else {
					printf("错误：压缩数量必须大于0\n");
				}
			    break;
		case'd':printf("输入文件名或文件地址：\n");
			if (fgets(filename, sizeof(filename), stdin) != NULL) {
			filename[strcspn(filename, "\n")] = '\0';
		}
			   else {
			  printf("读取文件失败\n");
			  continue;
		       }
			    quote_delete(filename);
			    generate_newname(filename, newfilename,0,filesuffix);
			    result=uncompress(filename,newfilename);
			    break;
		default:
			printf("错误：未知的命令\n");
		}
		if (result == 0) {
			printf("运行成功\n");
			printf("%s\n", newfilename);

		}
		else {
			printf("运行失败\n");
		}
	}
	return 0;
	}
