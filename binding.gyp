{
	"targets": [{
		"target_name": "json-tape",
		"sources": [
			"lib/json-asm.cc",
			"lib/json-tape.cc",
		],
		"include_dirs": [
			"deps/rapidjson/include"
		],

		"cflags" : [
			"-fexceptions",
			"-frtti",
			"-march=native",
			"-Wno-missing-field-initializers",
			"-std=c++11",
			"-Wno-reserved-id-macro",
			"-Wall",
			"-Weffc++",
			"-Wswitch-default",
			"-Wfloat-equal",
			"-Wimplicit-fallthrough",
			"-O3",
			"-g",
			"-DNDEBUG"
		],

		"xcode_settings": {
			"OTHER_CPLUSPLUSFLAGS": [
				"-fexceptions",
				"-frtti",
				"-march=native",
				"-Wno-missing-field-initializers",
				"-std=c++11",
				"-Wno-reserved-id-macro",
				"-Wall",
				"-Weffc++",
				"-Wswitch-default",
				"-Wfloat-equal",
				"-Wimplicit-fallthrough",
				"-O3",
				"-g",
				"-DNDEBUG"
			],
			"OTHER_LDFLAGS": [ "-stdlib=libc++" ],
			"MACOSX_DEPLOYMENT_TARGET": "10.11"
		}
	}]
}
