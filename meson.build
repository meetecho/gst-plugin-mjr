project('gst-template', 'c', version : '1.19.0.1', license : 'LGPL')

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

gst_version = meson.project_version()

api_version = '1.0'

gst_dep = dependency('gstreamer-1.0', version : '>=1.19',
	required : true, fallback : ['gstreamer', 'gst_dep'])
gstbase_dep = dependency('gstreamer-base-1.0', version : '>=1.19',
	fallback : ['gstreamer', 'gst_base_dep'])
json_dep = dependency('json-glib-1.0', version : '>=1.6.6', fallback : ['json-glib', 'json_glib_dep'], required : true)

plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', '0.0.1')
cdata.set_quoted('PACKAGE', 'gst-plugin-mjr')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'gst-plugin-mjr')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/meetecho/gst-plugin-mjr')
configure_file(output : 'config.h', configuration : cdata)

# The MJR plugins
gstmjr_sources = [
	'src/gstmjrplugin.c',
	'src/gstmjrdemux.c',
	'src/gstmjrmux.c',
	'src/gstmjrutils.c'
]

gstmjrexample = library('gstmjr',
	gstmjr_sources,
	c_args: plugin_c_args,
	dependencies : [gst_dep, gstbase_dep, json_dep],
	install : true,
	install_dir : plugins_install_dir,
)

