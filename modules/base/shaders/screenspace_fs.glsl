/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2021                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include "fragment.glsl"
#include "PowerScaling/powerScaling_fs.hglsl"

in vec2 vs_st;
in vec4 vs_position;

uniform sampler2D texture1;
uniform float Alpha;
uniform float time;

Fragment getFragment() {
    Fragment frag;

    frag.color = texture(texture1, vs_st);
    frag.color.a = Alpha * frag.color.a;
    if (frag.color.a == 0.0) {
        discard;
    }
	if(time!=0){
		if(vs_st.x>(0.133333+time))
			frag.color.a = (0.16666+time-vs_st.x)*30;
			//frag.color.a = 1.0-(vs_st.x-(0.4+time))*10;
		if(vs_st.x<(time-0.133333))
			//frag.color.a = (vs_st.x+(0.5-time))*10;
			frag.color.a = (vs_st.x-(time-0.166666))*30;
	}
    frag.depth = vs_position.z;
    return frag;
}
