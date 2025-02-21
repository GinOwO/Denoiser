#include "common.h"
#include "renderer.h"

#include <vector>
#include <string>

/*
Notation:

- S_: struct
- C_: class
- T_: template
- i_: integer
- f_: float
- b_: bool
- s_: string
- p_: pointer
- uX_: unsigned X int
- v_: std::vector
- vec_: glm::vec3

*/

int main()
{
	Renderer::Engine C_renderer{ 1024, 1024, 0.075f };

	std::string s_input_file =
		"/home/gin/Desktop/denoise/src/CornellBox.obj";
	std::string s_base_dir = "/home/gin/Desktop/denoise/src/";

	C_renderer.load_obj_scene(s_input_file, s_base_dir);

	C_renderer.render_loop();

	return EXIT_SUCCESS;
}
