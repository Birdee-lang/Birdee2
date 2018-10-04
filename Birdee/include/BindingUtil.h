#pragma once
#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;


using StatementASTList = std::vector<std::unique_ptr<Birdee::StatementAST>>;
PYBIND11_MAKE_OPAQUE(StatementASTList);
PYBIND11_MAKE_OPAQUE(std::vector<Birdee::TemplateArgument>);
PYBIND11_MAKE_OPAQUE(std::vector<std::unique_ptr<Birdee::ExprAST>>);
PYBIND11_MAKE_OPAQUE(std::vector<std::unique_ptr<Birdee::VariableSingleDefAST>>);
PYBIND11_MAKE_OPAQUE(std::vector<Birdee::TemplateParameter>);

//T can be clazz*/clazz/unique_ptr<clazz>
//"type" is the referenced type
//ToRef returns the "reference" to clazz
template<class T>
struct RefConverter
{
	using type = T;
	static T& ToRef(T& v)
	{
		return v;
	}

};

template<class T>
struct RefConverter<T*>
{
	using type = T;
	static T& ToRef(T* v)
	{
		return *v;
	}
};


template<class T>
struct RefConverter<std::unique_ptr<T>>
{
	using type = T;
	static T& ToRef(std::unique_ptr<T>& v)
	{
		return *v.get();
	}
};


template <class T>
auto GetNullRef()
{
	return std::reference_wrapper<T>(*(T*)nullptr);
}

//Converts some object reference/pointer/unique_ptr into reference_wrapper
template <class T>
auto GetRef(T& v)
{
	return std::reference_wrapper<T>(v);
}

template <class T>
auto GetRef(T* v)
{
	return std::reference_wrapper<T>(*v);
}

template <class T>
auto GetRef(std::unique_ptr<T>& v)
{
	return std::reference_wrapper<T>(*v);
}

//the iterator class for std::vector binding for python. 
//T can be clazz*/clazz/unique_ptr<clazz>
//will return reference_wrapper<clazz> for each access
template <class T>
struct VectorIterator
{
	size_t idx;
	std::vector <T>* vec;
	using type = typename RefConverter<T>::type;

	VectorIterator(std::vector<T>* vec, size_t idx) :vec(vec), idx(idx) {}

	VectorIterator(std::vector<T>* vec) :vec(vec) { idx = vec->size(); }

	VectorIterator& operator =(const VectorIterator& other)
	{
		vec = other.vec;
		idx = other.idx;
	}

	bool operator ==(const VectorIterator& other) const
	{
		return other.vec == vec && other.idx == idx;
	}
	bool operator !=(const VectorIterator& other) const
	{
		return !(*this == other);
	}
	VectorIterator& operator ++()
	{
		idx++;
		return *this;
	}
	static std::reference_wrapper<type> access(std::vector<T>& arr, int accidx)
	{
		return GetRef(arr[accidx]);
	}
	std::reference_wrapper<type> operator*() const
	{
		return access(*vec, idx);
	}
};


//registers the vector type & iterator
//T can be clazz*/clazz/unique_ptr<clazz>
template <class T>
void RegisiterObjectVector(py::module& m, const char* name)
{
	py::class_<std::vector<T>>(m, name)
		.def(py::init<>())
		.def("pop_back", &std::vector<T>::pop_back)
		.def("__getitem__", [](std::vector<T> &v, int idx) { return VectorIterator<T>::access(v, idx); })
		//.def("__setitem__", [](const StatementASTList &v, int idx) { return v[idx].get(); })
		.def("__len__", [](const std::vector<T> &v) { return v.size(); })
		.def("__iter__", [](std::vector<T> &v) {
		return py::make_iterator(VectorIterator<T>(&v, 0), VectorIterator<T>(&v));
	}, py::keep_alive<0, 1>());


}


//py::class_<TemplateParameters<FunctionAST>> does not seem to work? We need a fake class to bypass the bug
template <class T>
struct TemplateParameterFake
{
	Birdee::TemplateParameters<T>* get()
	{
		return (Birdee::TemplateParameters<T>*)this;
	}
};

template <class T>
void RegisiterTemplateParametersClass(py::module& m, const char* name)
{
	py::class_<TemplateParameterFake<T>>(m, name)
		.def_property_readonly("params", [](TemplateParameterFake<T>* ths) {return GetRef(ths->get()->params); })
		//fix-me: add instances, fix source.get()
		.def_property_readonly("source", [](TemplateParameterFake<T>* ths) {return ths->get()->source.get(); });

}
