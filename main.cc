#include <iostream>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <string>
#include <exception>
#include <bitset>
using namespace std;

// Нода дерева кодирования
struct Node
{
	char ch;
	int freq; // под частотой имеется в виду количество вхождений символа
	Node *left;
	Node *right;
};

// функция для гуя
void error_exit(string msg)
{
	cout << "ERROR: " << msg << "\n\n";
	cout << "Usage: archiver [OPTION] [FILE]\n";
	cout << "OPTIONS:\n\tc - compress\n\td - decompress\n";
	exit(EXIT_FAILURE);
}

// компаратор для приоритетной очереди
struct comparator
{
	bool operator()(const Node *l, const Node *r) const { return l->freq > r->freq; }
};

// для создания листов дерева
Node *create_node(char ch, int freq, Node *left = nullptr, Node *right = nullptr)
{
	Node *newnode = new Node();
	newnode->ch = ch;
	newnode->freq = freq;
	newnode->left = left;
	newnode->right = right;
	return newnode;
}

// рекурсивный обход дерева для получения кодов
void get_codes_map(Node *root, string str,
				   unordered_map<char, string> &codes_map)
{
	if (root == nullptr)
		return;

	// если нашли листок
	if (!root->left && !root->right)
	{
		codes_map[root->ch] = str;
	}

	get_codes_map(root->left, str + "0", codes_map);
	get_codes_map(root->right, str + "1", codes_map);
}

/// @brief функиция сжатия файла
/// @param filename имя файла, куда будут записаны сжатые данные
void compressor(const string filename)
{
	uint64_t symb_count = 0;

	ifstream f;
	f.open(filename);
	if (!f.is_open())
	{
		error_exit("no such file");
	}
	const int buf_max = 1000000;
	char *buf = new char[buf_max];
	f.read(buf, buf_max);
	string text = buf;

	// обычная мапа чисто посчитать
	unordered_map<char, int> symbols_freq;
	for (char ch : text)
	{
		symb_count++;
		symbols_freq[ch]++;
	}

	// приоритет - частота встреч по возрастанию
	priority_queue<Node *, vector<Node *>, comparator> pq;
	for (auto pair : symbols_freq)
	{
		pq.push(create_node(pair.first, pair.second)); // создание листов
	}

	// алгоритм хаффмана
	while (pq.size() != 1)
	{
		// забираем две буквы с минимальными весами
		Node *left = pq.top();
		pq.pop();
		Node *right = pq.top();
		pq.pop();

		// создаем новый узел из них
		int sum = left->freq + right->freq;
		pq.push(create_node('\0', sum, left, right));
	}

	// финальное дерево
	Node *root = pq.top();

	// сюда выгрузится таблица кодировки
	unordered_map<char, string> codes_map;
	get_codes_map(root, "", codes_map);

	cout << "Huffman Codes:\n";
	for (auto pair : codes_map)
	{
		if (pair.first == '\n')
			cout << '\"' << "\\n"
				 << "\" = " << pair.second << '\n';
		else
			cout << '\"' << pair.first << "\" = " << pair.second << '\n';
	}

	// запись кодировки в отдельный файл
	ofstream codesfile;
	codesfile.open(filename + ".cmp.codes");
	codesfile << symb_count << endl;
	for (auto pair : codes_map)
	{
		codesfile << (int)pair.first << ' ' << pair.second << endl;
	}
	codesfile.close();

	// кодируем данные
	string encoded_text;
	for (auto x : text)
	{
		encoded_text += codes_map[x];
	}
	cout << "Encoded string: " << encoded_text << endl;

	// пишем закодированные данные в файл
	ofstream ofile;
	ofile.open(filename + ".cmp", ios::binary);
	for (int i = 0; i < encoded_text.size(); i += 8)
	{
		// костыли для корректной записи в бинарном режиме
		int len = encoded_text.size() - i > 8 ? 8 : encoded_text.size() - i;
		bitset<8> bs((encoded_text.substr(i, len) + string(8 - len, '0')).c_str());

		char byte = bs.to_ulong();
		ofile << byte;
	}
	ofile.close();
}

void decompressor(const string filename)
{
	ifstream codefile;
	codefile.open(filename + ".codes");
	if (!codefile.is_open())
	{
		error_exit(filename + ".codes does not exist");
	}
	// код -> буква
	unordered_map<string, char> code_table;

	uint64_t symb_count;
	bool fst = true;
	while (!codefile.eof())
	{
		char buf[100];
		codefile.getline(buf, 100);
		if (buf[0] == '\0')
			break;
		string line = buf;
		if (fst)
		{
			symb_count = stoull(line);
			fst = false;
			continue;
		}

		// очередные костыли для парса строки
		char value = stoi(line.substr(0, line.find(' ')));
		string key = line.substr(line.find(' ') + 1, line.find('\n') - line.find(' ') + 1);

		code_table[key] = value;
	}

	codefile.close();

	ifstream in;
	in.open(filename);
	if (!in.is_open())
	{
		error_exit(filename + " does not exist");
	}
	string encoded_data = "";
	while (!in.eof())
	{
		encoded_data += bitset<8>(in.get()).to_string();
	}

	ofstream out;
	out.open(filename.substr(0, filename.rfind(".cmp")) + ".decmp");

	uint64_t curr = 0;
	int ptr = 0;
	int size = 1;
	while (curr != symb_count)
	{
		bool flag = true;
		string sub = encoded_data.substr(ptr, size);
		char ch;

		try
		{
			ch = code_table.at(sub);
		}
		catch (const std::exception &e)
		{
			flag = false;
		}

		if (flag)
		{
			out.put(ch);
			ptr = ptr + size;
			size = 1;
			curr++;
		}
		else
		{
			size += 1;
		}
	}
}

int main(int argc, char **argv)
{
	if (argc != 3)
	{
		error_exit("not enough args");
	}

	string option = argv[1];
	string filename = argv[2];

	if (option == "c")
	{
		compressor(filename);
	}

	if (option == "d")
	{
		decompressor(filename);
	}
}