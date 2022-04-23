/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.mini.jnibuilder;

import java.io.*;
import java.util.ArrayList;
import java.util.List;

/**
 * @author gust
 */
public class GL_h_2_java {

    public static void main(String[] args) {
        GL_h_2_java gt = new GL_h_2_java();
        gt.buildC();
    }

    String[] input_path = {
            "../c/deps/include/glad/glad.h",};
    String[] output_path = {"src/main/java/org/mini/gl/GL.java"};

    String CLASS_TEMPLATE
            = //
            "package org.mini.gl;\n"
                    + "/*this file generated by GL_h_2_java.java ,dont modify it manual.*/\n"
                    + "public class GL {\n"
                    + "\n"
                    + "    static {\n"
                    + "        org.mini.glfw.Glfw.loadLib();\n"
                    + "    }\n"
                    + "${FIELDS}\n"
                    + "    public static native void init();// //void\n"
                    + "${METHODS}\n"
                    + "}\n\n";
    String FIELDS = "${FIELDS}";
    String METHODS = "${METHODS}";

    String FIELD_TEMPLATE = "    public static final ${FIELD_TYPE} ${FIELD_NAME} = ${FIELD_VALUE};";
    String FIELD_NAME = "${FIELD_NAME}";
    String FIELD_TYPE = "${FIELD_TYPE}";
    String FIELD_VALUE = "${FIELD_VALUE}";

    String METHOD_DEC_TEMPLATE = "    public static native ${JAVA_RETURN} ${METHOD_NAME}(${JAVA_ARGV}); //${NATIVE_ARGV} //${NATIVE_RETURN}";
    String METHOD_NAME = "${METHOD_NAME}";
    String JAVA_ARGV = "${JAVA_ARGV}";
    String JAVA_RETURN = "${JAVA_RETURN}";

    String GET_VAR = "${GET_VAR}";
    //native
    //native
    String NATIVE_RETURN = "${NATIVE_RETURN}";
    String NATIVE_ARGV = "${NATIVE_ARGV}";
    String PUSH_RESULT = "${PUSH_RESULT}";

    static public String[] INT_TYPE = {"GLint", "GLuint", "GLenum", "GLbitfield", "GLboolean", "GLclampx", "GLsizei", "GLfixed",};
    static public String[] ARR_INT_TYPE = {"GLint*", "GLuint*", "GLenum*", "GLbitfield*", "GLboolean*", "GLclampx*", "GLsizei*", "GLfixed*", "const GLint*", "const GLuint*", "const GLenum*", "const GLbitfield*", "const GLboolean*", "const GLclampx*", "const GLsizei*", "const GLfixed*",};
    static public String[] SHORT_TYPE = {"GLshort", "GLushort",};
    static public String[] ARR_SHORT_TYPE = {"GLshort*", "const GLshort*", "GLushort*", "const GLushort*",};
    static public String[] BYTE_TYPE = {"GLbyte", "GLubyte", "GLchar", "GLcharARB",};
    static public String[] ARR_BYTE_TYPE = {"GLbyte*", "const GLbyte*", "GLubyte*", "const GLubyte*",};
    static public String[] STRING_TYPE = {"char*", "char*", "const char*", "const char*", "char const*", "char const*", "GLchar*", "const GLchar*", "GLcharARB*", "const GLcharARB*",};
    static public String[] ARR_STRING_TYPE = {"char**", "char**", "const char**", "const char**", "char const**", "char const**", "const GLchar*const*",};
    static public String[] LONG_TYPE = {"GLint64", "GLuint64", "GLsync", "GLDEBUGPROC", "GLDEBUGPROCKHR", "GLsizeiptr", "GLintptr",};
    static public String[] ARR_LONG_TYPE = {"GLint64*", "const GLint64*", "GLuint64*", "const GLuint64*", "GLsync*", "const GLsync*",};
    static public String[] FLOAT_TYPE = {"GLfloat", "GLclampf",};
    static public String[] ARR_FLOAT_TYPE = {"GLfloat*", "const GLfloat*", "GLclampf*", "const GLclampf*",};
    static public String[] DOUBLE_TYPE = {"GLdouble", "GLclampd",};
    static public String[] ARR_DOUBLE_TYPE = {"GLdouble*", "const GLdouble*", "GLclampd*", "const GLclampd*",};
    static public String[] OBJECT_TYPE = {"GLvoid*", "const GLvoid*", "GLsync*", "void*", "const void*",};
    static public String[] ARR_OBJECT_TYPE = {"GLvoid**", "const GLvoid**", "GLsync**", "const void*const*", "void**", "const void**", "void**",};
    static public String[] VOID_TYPE = {"GLvoid", "void",};
    static public String[] MULT_TYPE = {"...",};

    static public String[][] TYPES_ALL = {INT_TYPE, ARR_INT_TYPE, SHORT_TYPE, ARR_SHORT_TYPE, BYTE_TYPE, ARR_BYTE_TYPE, STRING_TYPE, ARR_STRING_TYPE, LONG_TYPE, ARR_LONG_TYPE, FLOAT_TYPE, ARR_FLOAT_TYPE, DOUBLE_TYPE, ARR_DOUBLE_TYPE, OBJECT_TYPE, ARR_OBJECT_TYPE, VOID_TYPE, MULT_TYPE};

    void buildC() {
        BufferedReader br = null;
        BufferedWriter bw = null;
        StringBuilder fields = new StringBuilder();
        StringBuilder methods = new StringBuilder();
        List<String> lines = new ArrayList();
        int lineNo = 0;
        try {

            String line;

            for (String filename : input_path) {
                File ifile = new File(filename);
                br = new BufferedReader(new FileReader(ifile));
                System.out.println("open input file:" + ifile.getAbsolutePath());
                while ((line = br.readLine()) != null) {

                    line = line.replaceAll("/\\*.*\\*/", "");
                    line = line.replaceAll("//.*\n", "");
                    line = line.trim();
                    lines.add(line);
                }
                br.close();
            }
            File ofile = new File(output_path[0]);
            bw = new BufferedWriter(new FileWriter(ofile));
            System.out.println("open output file:" + ofile.getAbsolutePath());

            //fields
            String nativeFieldHeader = "#define GL_";
            for (int i = 0, imax = lines.size(); i < imax; i++) {
                lineNo = i;
                line = lines.get(i);

                line = line.trim();
                lines.add(line);
                if (line.startsWith(nativeFieldHeader)) {
                    String fieldCode = FIELD_TEMPLATE;
                    String[] tmps = line.split(" ");
                    String typeCode = "int";
                    String value = tmps[2];
                    try {
                        String v = tmps[2].replace("0x", "");
                        Integer.parseInt(v, 16);
                    } catch (Exception e) {
                        typeCode = "long";
                        tmps[2] += "L";
                    }
                    fieldCode = fieldCode.replace(FIELD_TYPE, typeCode);
                    fieldCode = fieldCode.replace(FIELD_NAME, tmps[1]);
                    fieldCode = fieldCode.replace(FIELD_VALUE, tmps[2]);
                    fields.append(fieldCode);
                    fields.append("\n");
                }
            }

            //
            //methods
            String nativeMethodHeader = "#define gl";
            for (int i = 0, imax = lines.size(); i < imax; i++) {
                lineNo = i;
                line = lines.get(i);
                if (line.startsWith(nativeMethodHeader)) {
                    String output = METHOD_DEC_TEMPLATE;
                    String[] tmps = line.split(" ");
                    String javaNameCode = tmps[1];
                    String javaReturnCode = "";
                    String javaArgvCode = "";
                    String nativeCommentCode = "";

                    //native func str
                    String mdef = lines.get(i - 2);
                    if (i == 1405) {
                        int debug = 1;
                    }
                    String mtype = mdef.substring(mdef.indexOf(" "), mdef.indexOf("(")).trim();

                    //GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length
                    mdef = mdef.substring(mdef.lastIndexOf("(") + 1, mdef.lastIndexOf(")")).trim();
                    mdef = mdef.replace(" *", "*");

                    if (mdef.length() > 0) {
                        String[] nArgvs = mdef.split(",");
                        for (int j = 0; j < nArgvs.length; j++) {
                            String nargv = nArgvs[j].trim();
                            String arType = Util.getType(TYPES_ALL, nargv);
                            String arName = "arg" + j;
                            if (arType == null) {
                                System.out.println("error argv type:" + nargv);
                                continue;
                            }
                            if (arType.length() != nargv.length()) {
                                arName = nargv.substring(arType.length()).trim();
                            }

                            arName = arName.trim();
                            arName = "p" + arName;

                            if (Util.isTypes(INT_TYPE, arType)) {
                                javaArgvCode += "int " + arName;
                            } else if (Util.isTypes(ARR_INT_TYPE, arType)) {
                                javaArgvCode += "int[] " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(SHORT_TYPE, arType)) {
                                javaArgvCode += "short " + arName;
                            } else if (Util.isTypes(ARR_SHORT_TYPE, arType)) {
                                javaArgvCode += "short[] " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(BYTE_TYPE, arType)) {
                                javaArgvCode += "byte " + arName;
                            } else if (Util.isTypes(ARR_BYTE_TYPE, arType)) {
                                javaArgvCode += "byte[] " + arName;
//                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(STRING_TYPE, arType)) {
                                javaArgvCode += "byte[] " + arName;
                            } else if (Util.isTypes(ARR_STRING_TYPE, arType)) {
                                javaArgvCode += "byte[][] " + arName;
                                //no necessary offset
                            } else if (Util.isTypes(LONG_TYPE, arType)) {
                                javaArgvCode += "long " + arName;
                            } else if (Util.isTypes(ARR_LONG_TYPE, arType)) {
                                javaArgvCode += "long[] " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(FLOAT_TYPE, arType)) {
                                javaArgvCode += "float " + arName;
                            } else if (Util.isTypes(ARR_FLOAT_TYPE, arType)) {
                                javaArgvCode += "float[] " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(DOUBLE_TYPE, arType)) {
                                javaArgvCode += "double " + arName;
                            } else if (Util.isTypes(ARR_DOUBLE_TYPE, arType)) {
                                javaArgvCode += "double[] " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(OBJECT_TYPE, arType)) {
                                javaArgvCode += "Object " + arName;
                                javaArgvCode += ", int offset_" + arName;
                            } else if (Util.isTypes(ARR_OBJECT_TYPE, arType)) {
                                javaArgvCode += "Object[] " + arName;
                                //no necessary offset
                            } else if (Util.isTypes(MULT_TYPE, arType)) {
                                javaArgvCode += "String" + arType + " " + arName;
                            } else if (Util.isTypes(VOID_TYPE, arType)) {
                            } else {
                                System.out.println("java argv type:" + arType);
                            }
                            javaArgvCode += ", ";
                            nativeCommentCode += arType + ",";
                        }

                        int lastS = javaArgvCode.lastIndexOf(",");
                        if (lastS == javaArgvCode.length() - 2) {
                            javaArgvCode = javaArgvCode.substring(0, lastS);
                        }
                    }
                    //
                    mtype = mtype.replace(" *", "*");
                    if (Util.isTypes(INT_TYPE, mtype)) {
                        javaReturnCode = "int";
                    } else if (Util.isTypes(ARR_INT_TYPE, mtype)) {
                        javaReturnCode = "int[]";
                    } else if (Util.isTypes(FLOAT_TYPE, mtype)) {
                        javaReturnCode = "float";
                    } else if (Util.isTypes(ARR_FLOAT_TYPE, mtype)) {
                        javaReturnCode = "float[]";
                    } else if (Util.isTypes(LONG_TYPE, mtype)) {
                        javaReturnCode = "long";
                    } else if (Util.isTypes(ARR_BYTE_TYPE, mtype)) {
                        javaReturnCode = "byte[]";
                    } else if (Util.isTypes(STRING_TYPE, mtype)) {
                        javaReturnCode = "String";
                    } else if (Util.isTypes(DOUBLE_TYPE, mtype)) {
                        javaReturnCode = "double";
                    } else if (Util.isTypes(ARR_LONG_TYPE, mtype)) {
                        javaReturnCode = "long[]";
                    } else if (Util.isTypes(OBJECT_TYPE, mtype)) {
                        javaReturnCode = "long";
                    } else if (mtype.equals("void")) {
                        javaReturnCode = "void";
                    } else {
//                        javaReturnCode = "long";
                        System.out.println("java return :" + mtype);
                    }
                    String nativeReturnCode = mtype;
                    //result
                    output = output.replace(METHOD_NAME, javaNameCode);
                    output = output.replace(JAVA_RETURN, javaReturnCode);
                    output = output.replace(JAVA_ARGV, javaArgvCode);
                    output = output.replace(NATIVE_ARGV, nativeCommentCode);
                    output = output.replace(NATIVE_RETURN, nativeReturnCode);
                    if (methods.indexOf(javaNameCode + "(") < 0) {
                        methods.append(output);
                        methods.append("\n");
                    }
                }
            }

            String classCode = CLASS_TEMPLATE.replace(FIELDS, fields.toString());
            classCode = classCode.replace(METHODS, methods.toString());
            bw.write(classCode);
        } catch (Exception ex) {
            ex.printStackTrace();
            System.out.println("line no:" + lineNo);
        } finally {
            try {
                br.close();
                bw.close();
            } catch (IOException ex) {
                ex.printStackTrace();
            }
        }
        System.out.println("success.");
    }

}
