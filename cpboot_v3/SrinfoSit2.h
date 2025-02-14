/*
 * Copyright (C) 2020 Samsung Electronics
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

#pragma once

#include "Srinfo.h"

class SrinfoSit2 : public Srinfo {
public:
	SrinfoSit2();

protected:
	virtual void store_srinfo(int dev_fd) override;
	virtual shmem_srinfo *alloc_srinfo(unsigned *len, int boot_fd) override;

private:
	unsigned write_srinfo_file(int fd, char *buf, unsigned size) override;
};

