/**
 *  Variable.hpp
 *  ONScripter-RU
 *
 *  Variable entity support.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#pragma once

#include "External/Compatibility.hpp"

#include <cstdint>
#include <cstddef>
#include <cstdlib>

struct ArrayVariable {
	ArrayVariable *next{nullptr};
	int32_t no{0};
	int32_t num_dim{0};
	int32_t dim[20]{};
	int32_t *data{nullptr};
	ArrayVariable() = default;
	~ArrayVariable() {
		delete[] data;
	}
	ArrayVariable(const ArrayVariable &o) {
		*this = o;
	}
	ArrayVariable &operator=(const ArrayVariable &av) {
		if (this != &av) {
			no      = av.no;
			num_dim = av.num_dim;

			size_t total_dim = 1;
			for (int32_t i = 0; i < 20; i++) {
				dim[i] = av.dim[i];
				total_dim *= dim[i];
			}

			freearr(&data);
			if (av.data) {
				data = new int32_t[total_dim];
				std::memcpy(data, av.data, sizeof(int32_t) * total_dim);
			}
		}

		return *this;
	}
};

struct VariableInfo {
	enum {
		TypeNone  = 0,
		TypeInt   = 1, // integer
		TypeArray = 2, // array
		TypeStr   = 4, // string
		TypeConst = 8, // direct value or alias, not variable
		TypePtr   = 16 // pointer to a variable, e.g. i%0, s%0
	};

	int32_t type{TypeNone};
	int32_t var_no{0};   // for integer(%), array(?), string($) variable
	ArrayVariable array; // for array(?)
};

struct VariableData {
	int32_t num{0};
	bool num_limit_flag{false};
	int32_t num_limit_upper{0};
	int32_t num_limit_lower{0};
	char *str{nullptr};

	void reset(bool limit_reset_flag) {
		num = 0;
		if (limit_reset_flag)
			num_limit_flag = false;
		if (str) {
			delete[] str;
			str = nullptr;
		}
	}
};
