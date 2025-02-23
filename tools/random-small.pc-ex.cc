
#include "random-small.pc.h"
#include "random-small.pc-ex.h"

ex::Dummy::Dummy(const uint32_t* _data, const uint32_t* _end) {
	using _ = ::Dummy::_;
	protocache::Message _view(_data, _end);
	protocache::ExtractField(_view, _::ivec, _end, &ivec);
	protocache::ExtractField(_view, _::imap, _end, &imap);
};

protocache::Data ex::Dummy::Serialize() const {
	using _ = ::Dummy::_;
	std::vector<protocache::Data> parts(2);
	if (!ivec.empty()) {
		parts[_::ivec] = protocache::Serialize(ivec);
		if (parts[_::ivec].empty()) return {};
	}
	if (!imap.empty()) {
		parts[_::imap] = protocache::Serialize(imap);
		if (parts[_::imap].empty()) return {};
	}
	return protocache::SerializeMessage(parts);
}

