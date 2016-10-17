/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

class block_allocator {
private:
	struct block {
		size_t size;
		size_t used;
		char *buffer;
		block *next;
	};

	block *m_head;
	size_t m_blocksize;

	block_allocator(const block_allocator &);
	block_allocator &operator=(block_allocator &);

public:
	block_allocator(size_t blocksize);
	~block_allocator();

	// exchange contents with rhs
	void swap(block_allocator &rhs);

	// allocate memory
	void *malloc(size_t size);

	// free all allocated blocks
	void free();
};

#endif