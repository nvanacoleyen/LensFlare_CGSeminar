add_library(imgui
	"src/imgui.cpp"
	"src/imgui_demo.cpp"
	"src/imgui_draw.cpp"
	"src/imgui_tables.cpp"
	"src/imgui_widgets.cpp"
	"src/imgui_impl_opengl2.cpp"
	"src/imgui_impl_opengl3.cpp"
	"src/imgui_impl_glfw.cpp"
 "src/implot.cpp" "src/implot_demo.cpp"  "src/implot_items.cpp")
target_include_directories(imgui PUBLIC "include" PRIVATE "include/imgui" "include/implot")
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)