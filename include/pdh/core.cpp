#include <pdh/core.hpp>
#include <pdh/basic_impl.hpp>
#include <pdh/thread_Safe_impl.hpp>

namespace PDH {
	PDH::PDHImpl *PDHFactory::instance = NULL;

	void PDHFactory::set_threadSafe() {
		PDH::PDHImpl *old = instance;
		instance = new PDH::ThreadedSafePDH();
		delete old;
	}

	PDH::PDHImpl* PDHFactory::get_impl() {
		if (instance == NULL) {
			//instance = new PDH::NativeExternalPDH();
			instance = new PDH::ThreadedSafePDH();
		}
		return instance;
	}
}
