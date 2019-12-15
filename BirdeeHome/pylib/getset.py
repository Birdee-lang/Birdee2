from birdeec import *

def create_getter(clazz):
	for field in clazz.fields:
		if field.access==AccessModifier.PRIVATE:
			class_body(clazz,'''
public function get_{}() as {}
	return this.{}
end
'''.format(field.decl.name, field.decl.resolved_type,field.decl.name))

def create_setter(clazz):
	for field in clazz.fields:
		if field.access==AccessModifier.PRIVATE:
			class_body(clazz,'''
public function set_{}(__value__ as {})
	this.{}=__value__
end
'''.format(field.decl.name, field.decl.resolved_type,field.decl.name))