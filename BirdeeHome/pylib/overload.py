from birdeec import *

class ImplMatcher:
    def __init__(self, impl, args):
        self.args = args
        self.impl = impl
    
    def __str__(self):
        buf = [str(arg) for arg in self.args]
        return ', '.join(buf)
    '''
    Match the args (the actual template parameters of overloaded function) with
    the registered implementaions in self.
    Args should be list of TemplateArgument
    Returns the number of perfect matches of args with self.args, or None if the matching fails
    '''
    def match(self, args):
        if len(args) != len(self.args):
            return None
        matches = 0
        for (slf, tgt) in zip(self.args, args):
            if tgt.kind == TemplateArgument.TemplateArgumentType.TEMPLATE_ARG_TYPE:
                if slf != tgt.resolved_type:
                    #check if the given type can be auto-converted to the registered type
                    #if true, it is an inperfect match
                    if not tgt.resolved_type.is_convertible_to(slf):
                        return None
                else:
                    #this is a perfect match
                    matches+=1
            else:
                '''if slf.expr.resolved_type != tgt.expr.resolved_type:
                    return False
                if slf.expr.value != tgt.expr.value:
                    return False'''
                return None
        return matches
_mapping = dict()

def overloaded(func):
    if len(func.body)!=0:
        raise RuntimeError("The overloaded function must not have a body")
    if not func.is_template_instance:
        raise RuntimeError("The overloaded function must be a function template")
    templfunc = func.template_source_func
    if templfunc in _mapping:
        matchers = _mapping[templfunc]
        best_matcher = []
        best_num_matches = 0
        for matcher in matchers:
            _num_matches = matcher.match(func.template_instance_args)
            if _num_matches is not None:
                if best_num_matches < _num_matches:
                    best_num_matches = _num_matches
                    best_matcher = [matcher]
                elif best_num_matches == _num_matches:
                    best_matcher.append(matcher)
        if len(best_matcher)==1:
            def cls2name(cls):
                if cls:
                    return cls.name
                else:
                    return "NONE"               
            if func.proto.cls != best_matcher[0].impl.proto.cls:
                raise RuntimeError("Overriding function {}.{} with a function from a different class {}.{}".format(
                    cls2name(func.proto.cls), func.proto.name, cls2name(best_matcher[0].impl.proto.cls), best_matcher[0].impl.proto.name))
            if func.proto.cls:
                callee = MemberExprAST.new(ThisExprAST.new(), best_matcher[0].impl.proto.name)
            else:
                callee = ResolvedFuncExprAST.new(best_matcher[0].impl)
            callexpr = CallExprAST.new(callee, 
                [LocalVarExprAST.new(v) for v in func.proto.args])
            func.body.push_back(callexpr)
            return
        elif len(best_matcher)>1:
            curparams=', '.join([str(v.resolved_type) for v in func.proto.args])
            candidates='\n'.join(["{} @ {}".format(m, m.impl.pos) for m in best_matcher])
            raise RuntimeError("The overloaded function {} with parameter types ({}) has ambiguous overloading candidates:\n{}".format(
                templfunc.proto.name, curparams, candidates))
    raise RuntimeError("Cannot overload function {} with parameter types ({})".format(templfunc.proto.name,
        ', '.join([str(v.resolved_type) for v in func.proto.args])))

def overloading_with(target_name, *args):
    def rfunc(func):
        iden = IdentifierExprAST.new(target_name)
        thefunc = iden.get().impl
        if isinstance(thefunc, MemberExprAST):
            if thefunc.kind!=MemberExprAST.MemberType.FUNCTION and thefunc.kind!=MemberExprAST.MemberType.IMPORTED_FUNCTION and thefunc.kind!=MemberExprAST.MemberType.VIRTUAL_FUNCTION:
                raise RuntimeError("Expecting a function to overload with")
            if thefunc.kind==MemberExprAST.MemberType.IMPORTED_FUNCTION:
                thefunc = thefunc.imported_func
            else:
                thefunc = thefunc.func.decl
        elif isinstance(thefunc, ResolvedFuncExprAST):
            thefunc = thefunc.funcdef
        else:
            raise RuntimeError("The overloading target {} is not a function definition".format(target_name))
        resolved_args = []
        for arg in args:
            if isinstance(arg, str):
                resolved_args.append(resolve_type(arg))
            elif isinstance(arg, ResolvedType):
                resolved_args.append(arg)
            else:
                raise RuntimeError("The annotation 'overloading' only accept str or ResolvedType as type arguments")
        if thefunc not in _mapping:
            _mapping[thefunc] = []
        _mapping[thefunc].append(ImplMatcher(func, resolved_args))
    return rfunc

def overloading(target_name):
    def run(func):
        args=[arg.resolved_type for arg in func.proto.args]
        overloading_with(target_name, *args)(func)
    return run


