'tosfsmaker(create)_x86_64'

project_root = './'
binary_out = '../create.tosfs'
source_dir = 'src'
object_dir = 'obj'
include_dir = 'include'
object_ext = 'o'
object_main = 'main.o'

{
	include_type = 'lib local'
	include_local = '#include "%s"'
	include_lib = '#include <%s>'
	source_ext = 'cpp'
	compiler_command = 'g++ -m64 -c -o {output} {input} -I {include} -masm=intel -g'
}

linker_command = 'g++ -m64 -o {output} {input} -g -lboost_filesystem'
