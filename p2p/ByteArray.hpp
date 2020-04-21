#ifndef BYTE_ARRAY_HPP
#define BYTE_ARRAY_HPP
 
#include <cstdio>
#include <cstdint>
#include <vector>

class ByteArray
{
public:	
	ByteArray();
	ByteArray(const ByteArray &ba);
	ByteArray(const char *data, int size);
	ByteArray(const std::string& content);
	virtual ~ByteArray();

	inline uint8_t* Data();
	inline int Size();

	inline void Write(const char* data, int size);
	inline int  Read(void* data, int size);
	inline void Seek(int pos);

	inline void WriteUint16BE(uint16_t value);
	inline void WriteUint24BE(uint32_t value);
	inline void WriteUint32BE(uint32_t value);
	inline void WriteUint24LE(uint32_t value);
	inline void WriteUint32LE(uint32_t value);
	inline void WriteUint16LE(uint16_t value);
	
	inline uint16_t ReadUint16LE();
	inline uint32_t ReadUint24LE();
	inline uint32_t ReadUint32LE();
	inline uint16_t ReadUint16BE();
	inline uint32_t ReadUint24BE();
	inline uint32_t ReadUint32BE();

private:
	inline void resize(int size);

	int index_ = 0;
	int size_  = 0;
	std::vector<uint8_t> data_;
};

ByteArray::ByteArray()
{
	data_.resize(0);
}

ByteArray::ByteArray(const ByteArray &ba)
{
	index_ =  ba.index_;
	size_  =  ba.size_;
	data_  =  ba.data_;
}

ByteArray::ByteArray(const char* data, int size)
{
	index_ = 0;
	size_ = size;
	data_.resize(size_);
	if (size_ > 0) {
		memcpy(&data_[0], data, size_);
	}
}

ByteArray::ByteArray(const std::string& content)
{
	index_ = 0;
	size_ = (int)content.size();
	data_.resize(size_);

	if (size_ > 0) {
		memcpy(&data_[0], content.c_str(), size_);
	}
}

ByteArray::~ByteArray()
{
	index_ = 0;
	size_  = 0;
}

inline uint8_t* ByteArray::Data()
{
	return &data_[0];
}

inline int ByteArray::Size()
{
	return size_;
}

inline void ByteArray::resize(int new_size)
{
	if (size_ - index_ < new_size) {
		data_.resize(size_ + new_size);
		size_ += new_size;
	}
}

inline void ByteArray::Write(const char *data, int size)
{
	resize(size);
	memcpy(&data_[index_], data, size);
	index_ += size;
}

inline int ByteArray::Read(void* data, int size)
{	
	int cap = size - index_;
	if (size > cap) {
		size = cap;
	}
	else {
		return 0;
	}

	memcpy(data, &data_[0], size);
	index_ += size;
	return size;
}

inline void ByteArray::Seek(int pos)
{
	index_ = pos;

	if (index_ > size_) {
		index_ = size_;
	}

	if (index_ < 0) {
		index_ = 0;
	}
}

inline void ByteArray::WriteUint16BE(uint16_t value)
{
	resize(2);
	data_[index_++] = value >> 8;
	data_[index_++] = value &  0xff;
}

inline void ByteArray::WriteUint24BE(uint32_t value)
{
	resize(3);
	data_[index_++] = value >> 16;
	data_[index_++] = value >> 8;
	data_[index_++] = value &  0xff;
}

inline void ByteArray::WriteUint32BE(uint32_t value)
{
	resize(4);
	data_[index_++] = value >> 24;
	data_[index_++] = value >> 16;
	data_[index_++] = value >> 8;
	data_[index_++] = value &  0xff;
}

inline void ByteArray::WriteUint16LE(uint16_t value)
{
	resize(2);
	data_[index_++] = value & 0xff;
	data_[index_++] = value >> 8;
}

inline void ByteArray::WriteUint24LE(uint32_t value)
{
	resize(3);
	data_[index_++] = value &  0xff;
	data_[index_++] = value >> 8;
	data_[index_++] = value >> 16;
}

inline void ByteArray::WriteUint32LE(uint32_t value)
{
	resize(4);
	data_[index_++] = value &  0xff;
	data_[index_++] = value >> 8;
	data_[index_++] = value >> 16;
	data_[index_++] = value >> 24;
}

inline uint32_t ByteArray::ReadUint32BE()
{
	uint32_t value = 0;
	if (size_ - index_ < 4) {
		return value;
	}

	value = (data_[index_] << 24) | (data_[index_ + 1] << 16) | (data_[index_ + 2] << 8) | data_[index_ + 3];
	index_ += 4;
	return value;
}

inline uint32_t ByteArray::ReadUint32LE()
{
	uint32_t value = 0;
	if (size_ - index_ < 4) {
		return value;
	}

	value = (data_[index_+3] << 24) | (data_[index_ + 2] << 16) | (data_[index_ + 1] << 8) | data_[index_];
	index_ += 4;
	return value;
}

inline uint32_t ByteArray::ReadUint24BE()
{
	uint32_t value = 0;
	if (size_ - index_ < 3) {
		return value;
	}

	value = (data_[index_] << 16) | (data_[index_ + 1] << 8) | data_[index_ + 2];
	index_ += 3;
	return value;
}

inline uint32_t ByteArray::ReadUint24LE()
{
	uint32_t value = 0;
	if (size_ - index_ < 3) {
		return value;
	}

	value = (data_[index_ + 2] << 16) | (data_[index_ + 1] << 8) | data_[index_];
	index_ += 3;
	return value;
}

inline uint16_t ByteArray::ReadUint16BE()
{
	uint32_t value = 0;
	if (size_ - index_ < 2) {
		return value;
	}

	value = (data_[index_] << 8) | data_[index_ + 1];
	index_ += 2;
	return value;
}

inline uint16_t ByteArray::ReadUint16LE()
{
	uint32_t value = 0;
	if (size_ - index_ < 2) {
		return value;
	}

	value = (data_[index_ + 1] << 8) | data_[index_];
	index_ += 2;
	return value;
}


#endif