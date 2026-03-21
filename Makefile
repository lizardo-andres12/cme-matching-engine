BUILD := build
LIB := lib

TEST_BIN := run_tests

SBE_XML_DIR := xml
SBE_PATH := $(LIB)/simple-binary-encoding
SBE_TOOL_VERSION := 1.38.0-SNAPSHOT

test:
	@./${BUILD}/${TEST_BIN}

xmlgen:
	@java --add-opens java.base/jdk.internal.misc=ALL-UNNAMED -Dsbe.generate.ir=true -Dsbe.target.language=Cpp -Dsbe.target.namespace=sbe -Dsbe.output.dir=include/gen -Dsbe.errorLog=yes -jar $(SBE_PATH)/sbe-all/build/libs/sbe-all-${SBE_TOOL_VERSION}.jar $(SBE_XML_DIR)/templates_FixBinary.xml
