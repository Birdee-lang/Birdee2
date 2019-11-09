from bdutils import *

def autoclose(var):
	if not isinstance(var, VariableSingleDefAST) or var.value is None:
		raise RuntimeError("autoclose can only be applied on single variable definitions with initial values")
	init_v = var.value
	replacedPtr = ScriptAST.new("", False)
	replaced = replacedPtr.get()
	replaced.stmt.push_back(init_v.copy())
	replaced.stmt.push_back(stmt('''defer
		{name}.close()
	end
	'''.format(name=var.name)))
	replaced.resolved_type = init_v.resolved_type
	var.value = replacedPtr