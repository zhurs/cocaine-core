//
// Copyright (C) 2011-2012 Rim Zaidullin <creator@bash.org.ru>
//
// Licensed under the BSD 2-Clause License (the "License");
// you may not use this file except in compliance with the License.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <cstring>

#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include <uuid/uuid.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include "json/json.h"

#include "cocaine/dealer/details/error.hpp"
#include "cocaine/dealer/details/persistant_data_container.hpp"

namespace cocaine {
namespace dealer {

persistant_data_container::persistant_data_container() :
	size_(0),
	signed_(false)
{
	init();
}

persistant_data_container::persistant_data_container(const void* data, size_t size) :
	size_(size),
	signed_(false)
{
	init_with_data((unsigned char*)data, size);
}

persistant_data_container::persistant_data_container(const persistant_data_container& dc) {
	if (dc.empty()) {
		init();
		return;
	}

	blob_ = dc.blob_;
	size_ = dc.size_;

	if (dc.signed_) {
		memcpy(signature_, dc.signature_, SHA1_SIZE);
		signed_ = dc.signed_;
	}

	ref_counter_ = dc.ref_counter_;
	++*ref_counter_;
}

void
persistant_data_container::init_with_data(unsigned char* data, size_t size) {
	init();

	if (data == NULL || size == 0) {
		size_ = 0;
		return;
	}

	std::string error_msg = "not enough memory to create new data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	/*
	// allocate new space
	try {
		data_ = new unsigned char[size];
	}
	catch (...) {
		throw error(error_msg);
	}

	if (!data_) {
		throw error(error_msg);
	}

	// copy data
	memcpy(data_, data, size);
	++*ref_counter_;

	size_ = size;

	if (size <= SMALL_DATA_SIZE) {
		return;
	}

	sign_data(data_, size_, signature_);
	signed_ = true;
	*/
}

void
persistant_data_container::init() {
	// reset sha1 signature
	signed_ = false;
	memset(signature_, 0, SHA1_SIZE);

	// create ref counter
	std::string error_msg = "not enough memory to create new ref_counter in data container at ";
	error_msg += std::string(BOOST_CURRENT_FUNCTION);

	try {
		ref_counter_.reset(new reference_counter(0));
	}
	catch (...) {
		throw error(error_msg);
	}

	if (!ref_counter_.get()) {
		throw error(error_msg);
	}

	// init data
	//data_ = NULL;
	size_ = 0;
}

persistant_data_container::~persistant_data_container() {
	if (*ref_counter_ == 0) {
		return;
	}

	--*ref_counter_;

	//if (data_ && *ref_counter_ == 0) {
	//	delete [] data_;
	//}
}

void
persistant_data_container::swap(persistant_data_container& other) {
	//std::swap(data_, other.data_);
	std::swap(size_, other.size_);
	std::swap(signed_, other.signed_);

	unsigned char signature_tmp[SHA1_SIZE];
	memcpy(signature_tmp, signature_, SHA1_SIZE);
	memcpy(signature_, other.signature_, SHA1_SIZE);
	memcpy(other.signature_, signature_tmp, SHA1_SIZE);

	std::swap(ref_counter_, other.ref_counter_);
}

void
persistant_data_container::copy(const persistant_data_container& other) {
	//data_ = other.data_;
	size_ = other.size_;
	signed_ = other.signed_;

	memcpy(signature_, other.signature_, SHA1_SIZE);
	ref_counter_ = other.ref_counter_;
	++*ref_counter_;
}

persistant_data_container&
persistant_data_container::operator = (const persistant_data_container& rhs) {
	boost::mutex::scoped_lock lock(mutex_);

	this->copy(rhs);
	return *this;
}

bool
persistant_data_container::operator == (const persistant_data_container& rhs) const {
	// data size differs?
	if (size_ != rhs.size_) {
		return false;
	}

	// both containers empty?
	if (size_ == 0 && rhs.size_ == 0) {
		return true;
	}

	// compare small containers
	//if (size_ <= SMALL_DATA_SIZE) {
	//	return (0 == memcmp(data_, rhs.data_, size_));
	//}

	// compare big containers
	return (0 == memcmp(signature_, rhs.signature_, SHA1_SIZE));
}

bool
persistant_data_container::operator != (const persistant_data_container& rhs) const {
	return !(*this == rhs);
}

bool
persistant_data_container::empty() const {
	return (size_ == 0);
}

void
persistant_data_container::clear() {
	boost::mutex::scoped_lock lock(mutex_);

	if (*ref_counter_ == 0) {
		return;
	}

	--*ref_counter_;

	//if (data_ && *ref_counter_ == 0) {
	//	delete data_;
	//}

	init();
}

void*
persistant_data_container::data() const {
	//return (void*)data_;
}

size_t
persistant_data_container::size() const {
	//return size_;
}

void
persistant_data_container::sign_data(unsigned char* data, size_t& size, unsigned char signature[SHA1_SIZE]) {
	SHA_CTX sha_context;
	SHA1_Init(&sha_context);

	// reset signature
	memset(signature, 0, sizeof(signature));

	// go through compiled and flattened data in chunks of 512 kb., building signature
	unsigned int full_chunks_count = size / SHA1_CHUNK_SIZE;
	char* data_offset_ptr = (char*)data;

	// process full chunks
	for (unsigned i = 0; i < full_chunks_count; i++) {
		SHA1_Update(&sha_context, data_offset_ptr, SHA1_CHUNK_SIZE);
		data_offset_ptr += SHA1_CHUNK_SIZE;
	}

	unsigned int remainder_chunk_size = 0;

	if (size < SHA1_CHUNK_SIZE) {
		remainder_chunk_size = size;
	}
	else {
		remainder_chunk_size = size % SHA1_CHUNK_SIZE;
	}

	// process remained chunk
	if (0 != remainder_chunk_size) {
		SHA1_Update(&sha_context, data_offset_ptr, remainder_chunk_size);
	}

	SHA1_Final(signature, &sha_context);
}

void
persistant_data_container::set_eblob(eblob blob) {
	blob_ = blob;
}

} // namespace dealer
} // namespace cocaine
