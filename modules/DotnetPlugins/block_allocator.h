/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
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