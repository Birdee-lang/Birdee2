#pragma once
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <Util.h>
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


template<class T>
struct UniquePtr
{
	T ptr;
	auto get()
	{
		return GetRef(ptr);
	}
	T move() { return std::move(ptr); }
	std::unique_ptr<Birdee::ExprAST> move_expr() {
		assert(0 && "Do not call move_expr for T != unique_ptr<StatementAST>");
		return nullptr;//make g++ happy
	}
	void init() {}
	UniquePtr(T&& ptr) :ptr(std::move(ptr)) { init(); }
};

template<>
inline void UniquePtr< std::unique_ptr<Birdee::StatementAST>>::init()
{
	ptr->Phase1();
}
template<>
inline std::unique_ptr<Birdee::StatementAST> UniquePtr< std::unique_ptr<Birdee::StatementAST>>::move()
{
	if(ptr==nullptr)
		throw std::invalid_argument("the contained pointer is already moved!");
	return std::move(ptr);
}


template<typename Derived, typename Base>
std::unique_ptr<Derived> move_cast_or_throw(std::unique_ptr<Base>& ptr)
{
	if (!Birdee::instance_of<Derived>(ptr.get()))
		throw std::invalid_argument("the contained pointer is not an Derived type!");
	return Birdee::unique_ptr_cast<Derived>(std::move(ptr));
}
template<>
inline std::unique_ptr<Birdee::ExprAST> UniquePtr< std::unique_ptr<Birdee::StatementAST>>::move_expr()
{
	return move_cast_or_throw< Birdee::ExprAST>(ptr);
}

using UniquePtrStatementAST = UniquePtr<std::unique_ptr<Birdee::StatementAST>>;

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
		.def("push_back", [](std::vector<T> &v, UniquePtr<T>* ptr) { v.push_back(ptr->move()); })
		.def("__getitem__", [](std::vector<T> &v, int idx) { return VectorIterator<T>::access(v, idx); })
		.def("__setitem__", [](std::vector<T> &v, int idx, UniquePtr<T>* ptr) { v[idx] = ptr->move(); })
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
