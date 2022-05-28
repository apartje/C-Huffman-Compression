//author: apartje

#include <string>
#include <string_view>

#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <chrono>


class c_huffman
{
	struct t_node
	{
		t_node* m_parent;
		t_node* m_node[2];
		uint8_t m_value;
		uint32_t m_freq;
	};
public:
	struct t_key_pair
	{
		uint8_t m_value;
		uint32_t m_freq;
		t_key_pair(const uint8_t arg_value, const uint32_t arg_freq)
		{
			m_value = arg_value;
			m_freq = arg_freq;
		}
	};
private:
	std::vector<uint8_t> m_buffer;

	t_node* create_tree(uint32_t* arg_keypair, t_node** arg_translate)
	{
		std::vector<t_node*> queue;

		for (auto x = 0; x < (0xff + 1); x++)
		{
			const auto freq = arg_keypair[x];
			if (freq == 0)
				continue;

			auto node = new t_node();
			node->m_value = x;
			node->m_freq = freq;
			node->m_parent = nullptr;

			arg_translate[node->m_value] = node;

			queue.push_back(node);
		}

		while (true)
		{
			if (queue.size() == 1)
				break;

			std::ranges::sort(queue, [](const t_node* l, const t_node* r) {return l->m_freq > r->m_freq; });

			auto next_node = new t_node();
			const auto left = queue.back(); queue.pop_back();
			const auto right = queue.back(); queue.pop_back();

			left->m_parent = next_node;
			right->m_parent = next_node;

			next_node->m_node[0] = left;
			next_node->m_node[1] = right;

			next_node->m_freq = left->m_freq + right->m_freq;

			queue.push_back(next_node);
		}

		//fix single char compression
		if (queue.back()->m_node[0] == nullptr && queue.back()->m_node[1] == nullptr)
		{
			const auto next_node = new t_node();
			queue.back()->m_parent = next_node;
			next_node->m_node[0] = queue.back();

			return next_node;
		}

		return queue.back();
	}

	void destroy_tree(const t_node* arg_node)
	{
		for (const auto x : arg_node->m_node)
		{
			if (x != nullptr)
			{
				destroy_tree(x);
				delete[] x;
			}
		}
	}

	void compress_bit(uint64_t& arg_index, const t_node* arg_node)
	{
		while (true)
		{
			if (arg_node->m_parent == nullptr)
			{
				break;
			}

			const uint64_t old_index = arg_index;
			arg_index++;

			//should not happen
			if (m_buffer.size() <= (arg_index / 8))
			{
				m_buffer.resize(m_buffer.size() + 0xfff);
			}

			const auto parent = arg_node;
			arg_node = arg_node->m_parent;

			if (arg_node->m_node[0] == parent)
				m_buffer[old_index / 8] |= (0x1 << (old_index % 8));
			else
				m_buffer[old_index / 8] &= ~(0x1 << (old_index % 8));
		}
	}
	bool decompress_bit(t_node** arg_node, bool bit, std::vector<uint8_t>& arg_out)
	{

		*arg_node = (*arg_node)->m_node[bit];
		if ((*arg_node)->m_node[bit] == nullptr)
		{
			arg_out.push_back((*arg_node)->m_value);
			return true;
		}

		return false;
	}
public:
	c_huffman() = default;

	bool create_key_pair(const uint8_t* arg_data, const uint32_t arg_size, uint32_t* arg_out)
	{
		if (arg_size == 0)
			return false;

		memset((void*)arg_out, 0, (0xff + 1) * 4);
		for (auto x = 0; x < arg_size; x++)
		{
			arg_out[arg_data[x]]++;
		}

		return true;
	}

	bool compress(const uint8_t* arg_data, const uint32_t arg_size, uint32_t* arg_keypair, std::vector<uint8_t>& arg_out)
	{
		if (arg_size == 0)
			return false;

		t_node* leaf_map[0xff + 1];
		const auto tree = create_tree(arg_keypair, leaf_map);

		m_buffer.resize(std::max(static_cast<uint32_t>(100), arg_size));
		const uint32_t pos = arg_size - 1;

		//temp fix for integer overflow on bigger array sizes
		uint64_t index = 0;
		for (uint32_t x = 0; x < arg_size; ++x)
		{
			const auto val = arg_data[pos - x];
			compress_bit(index, leaf_map[val]);
		}

		m_buffer.resize((index / 8) + 1);

		arg_out = m_buffer;

		//push bit count to buffer
		const auto base = reinterpret_cast<uint8_t*>(&index);
		for (int i = 0; i < 8; i++) {
			arg_out.push_back(base[i]);
		}

		//push keymap to buffer
		//...

		destroy_tree(tree);
		delete[] tree;

		return true;
	}


	bool decompress(uint8_t* arg_data, uint32_t arg_size, uint32_t* arg_keypair, std::vector<uint8_t>& arg_out)
	{
		if (arg_size == 0)
			return false;

		t_node* leaf_map[0xff + 1];
		const auto tree = create_tree(arg_keypair, leaf_map);

		arg_out.reserve(arg_size);

		auto temp_node = tree;

		const uint64_t bit_count = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(&arg_data[arg_size - 8]));

		const uint64_t pos = bit_count - 1;
		for (uint64_t i = 0; i < bit_count; i++) {
			const auto v = pos - i;
			if (decompress_bit(&temp_node, !(arg_data[v / 8] >> v % 8 & 0x1), arg_out))
			{
				temp_node = tree;
			}
		}

		destroy_tree(tree);
		delete[] tree;

		return true;
	}

};

const int size = (1024 * 1024 * 1024) * 1;


#include <Windows.h>
int main()
{
	char* data = nullptr;
	data = (char*)VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	for (auto x = 0; x < size; ++x)
		data[x] = x % 0xff;

	std::cout << "Huffman Compression: \n";

	//setup
	c_huffman huffman;

	uint32_t key_pair[0xff + 1];
	huffman.create_key_pair((uint8_t*)data, size, key_pair);


	//compress
	std::vector<uint8_t> compressed_data;
	huffman.compress((uint8_t*)data, size, key_pair, compressed_data);

	auto t1 = std::chrono::high_resolution_clock::now();
	//decompress
	std::vector<uint8_t> uncoompressed_data;
	huffman.decompress(compressed_data.data(), compressed_data.size(), key_pair, uncoompressed_data);


	auto t2 = std::chrono::high_resolution_clock::now();
	std::cout << duration_cast<std::chrono::milliseconds>(t2 - t1).count() << " ms speed\n";


	std::cout << "Orig Size:" << size << "\n";
	std::cout << "Compressed Size:" << compressed_data.size() << "\n";
	std::cout << "Restored Size:" << uncoompressed_data.size() << "\n";

	//std::cout << "Data:" << "\n";
	//std::cout << std::string_view(reinterpret_cast<char*>(uncoompressed_data.data()), uncoompressed_data.size()) << "\n";

	std::cin.get();
}

