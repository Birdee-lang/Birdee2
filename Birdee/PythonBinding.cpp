#include <pybind11/embed.h>
#include "BdAST.h"
#include "Tokenizer.h"

namespace py = pybind11;
using namespace Birdee;

extern Tokenizer SwitchTokenizer(Tokenizer&& tokzr);
PYBIND11_EMBEDDED_MODULE(birdeec, m) {
	// `m` is a `py::module` which is used to bind functions and classes
	m.def("exec", [](char* cmd) {
		Tokenizer toknzr(std::make_unique<StringStream>(string(cmd)), source_paths.size() - 1);
		return py::exec(cmd);
	});
}
