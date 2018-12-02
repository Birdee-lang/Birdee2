from birdeec import *

def create_getter(clazz):
	for field in clazz.fields:
		if field.access==AccessModifier.PRIVATE:
			class_body(clazz,f'''
public function get_{field.decl.name}() as {field.decl.resolved_type}
	return this.{field.decl.name}
end
''')

def create_setter(clazz):
	for field in clazz.fields:
		if field.access==AccessModifier.PRIVATE:
			class_body(clazz,f'''
public function set_{field.decl.name}(__value__ as {field.decl.resolved_type})
	this.{field.decl.name}=__value__
end
''')