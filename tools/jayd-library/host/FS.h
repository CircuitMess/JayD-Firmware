#pragma once

#include <fstream>
#include <memory>
#include <stdint.h>
#include <string>

namespace fs {

class File {
public:
	File() = default;

	explicit File(const char* path) : state_(std::make_shared<State>(path)) {
		if(!state_->stream) state_.reset();
	}

	explicit operator bool() const {
		return state_ && state_->stream.is_open();
	}

	size_t size() {
		return state_ ? state_->size : 0;
	}

	bool seek(uint32_t position) {
		if(!state_) return false;
		state_->stream.clear();
		state_->stream.seekg(position);
		return bool(state_->stream);
	}

	size_t read(uint8_t* output, size_t size) {
		if(!state_) return 0;
		state_->stream.read(reinterpret_cast<char*>(output), size);
		return static_cast<size_t>(state_->stream.gcount());
	}

	void close() {
		if(state_) state_->stream.close();
		state_.reset();
	}

private:
	struct State {
		explicit State(const char* path) : stream(path, std::ios::binary), size(0) {
			if(!stream) return;
			stream.seekg(0, std::ios::end);
			size = static_cast<size_t>(stream.tellg());
			stream.seekg(0);
		}

		std::ifstream stream;
		size_t size;
	};

	std::shared_ptr<State> state_;
};

} // namespace fs
