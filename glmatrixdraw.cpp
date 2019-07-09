//
// Created by Olaf Leidinger <oleid@mescharet.de> on 08.07.19.
//

#include "glmatrixdraw.h"

struct UploadJob
{
	int start_row_idx;
	int current_row_idx;
	std::vector<std::shared_ptr<std::vector<float>>> data;
};

namespace detail
{
texConf::texConf(float)
	: bytes_per_val(sizeof(GLfloat))
	, n_primitives_per_val(1)
	, internal_format(GL_R32F)
	, pixel_format(GL_RED)
	, pixel_type(GL_FLOAT)
{
}

texConf::texConf(std::complex<float>)
	: bytes_per_val(2 * sizeof(GLfloat))
	, n_primitives_per_val(2)
	, internal_format(GL_RG32F)
	, pixel_format(GL_RG)
	, pixel_type(GL_FLOAT)
{
}

} // namespace detail

GlMatrixDraw::GlMatrixDraw(size_t rows, size_t cols, detail::texConf config)
	: is_radar_plot(false)
	, tex_width(cols)
	, tex_height(rows)
	, input_data_len(tex_width)
	, upload_idx(1)
	, copy_idx(0)
	, upload_time(0.0)
	, copy_time(0.0)
	, time_cnt(0)
	, input_qs{{UploadQueue(tex_height), UploadQueue(tex_height)}}
	, n_paint(0)
	, conf(config)
{
}

GlMatrixDraw::~GlMatrixDraw() { cleanup(); }

template <typename T>
void scale_by_factor(std::vector<T>& input, int factor)
{
	const int N = static_cast<int>(input.size());
	assert(factor > 0);
	auto cmp = [](auto a, auto b) -> bool { return (std::abs(a) < std::abs(b)); };
	for (int i = 0; i < N; i += factor)
	{
		const auto max = std::max_element(input.begin() + i, input.begin() + i + factor, cmp);
		input[i / factor] = *max;
	}
	input.resize(input.size() / factor);
}

GlMatrixDraw::Row GlMatrixDraw::maybe_scale_data(Row&& input) const
{
	const int target_len = static_cast<int>(conf.n_primitives_per_val * tex_width);
	const int N = static_cast<int>(input.size());
	if (N != target_len)
	{
		const int factor = std::round(N / target_len);
		switch (conf.n_primitives_per_val)
		{
		case 1:
			scale_by_factor(input, factor);
			break;
		case 2:
		{
			auto& cpx_input = *reinterpret_cast<std::vector<std::complex<float>>*>(&input);
			scale_by_factor(cpx_input, factor);
			break;
		}
		default:
			std::cerr << "I don't know how to scale this input!\n";
			break;
		}
		assert(static_cast<int>(input.size()) == target_len);
	}
	return input;
}

bool GlMatrixDraw::insert(int pos, std::vector<float> input)
{
	if (pos >= tex_height)
	{
		return false;
	}
	assert(input.size() == conf.n_primitives_per_val * static_cast<size_t>(input_data_len));
	assert(input.size() % conf.n_primitives_per_val == 0);

	auto input_ptr = std::make_shared<Row>(maybe_scale_data(std::move(input)));

	using QEntry = typename UploadQueue::Entry;
	input_qs[0].input_q.enqueue(QEntry{append_pos, input_ptr});
	input_qs[1].input_q.enqueue(QEntry{append_pos, input_ptr});
	return true;
}

void GlMatrixDraw::cleanup()
{
	if (!m_program)
		return;

	// clean up texture
	glDeleteTextures(1, &textureId);

	// clean up PBOs
	for (auto& el : input_qs)
	{
		glDeleteBuffers(1, &el.pbo_id);
	}
}

/// A vertex shader, which generates our full screen triangle
/// and it's texture
static const char* vertexShaderSource = "#version 330\n"
										"out vec2 texCoord;\n"
										" \n"
										"void main()\n"
										"{\n"
										"    float x = -1.0 + float((gl_VertexID & 1) << 2);\n"
										"    float y = -1.0 + float((gl_VertexID & 2) << 1);\n"
										"    texCoord.x = (y+1.0)*0.5;\n"
										"    texCoord.y = (x+1.0)*0.5;\n"
										"    gl_Position = vec4(x, y, 0, 1);\n"
										"}";

void GlMatrixDraw::initializeGL()
{
	// In this example the widget's corresponding top-level window can change
	// several times during the widget's lifetime. Whenever this happens, the
	// QOpenGLWidget's associated context is destroyed and a new one is created.
	// Therefore we have to be prepared to clean up the resources on the
	// aboutToBeDestroyed() signal, instead of the destructor. The emission of
	// the signal will be followed by an invocation of initializeGL() where we
	// can recreate all resources.
	initializeOpenGLFunctions();

	// If your matrix is huge _or_ your GPU really old, data might not fit. Check.
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

	tex_height = std::min(tex_height, max_texture_size); // row count is flexible

	if (tex_width > max_texture_size)
	{
		// Choose texture with so that scaling input data is an integer factor
		tex_width = std::floor(input_data_len / std::round(input_data_len / max_texture_size));
	}

	glClearColor(0, 0, 0, 1);

	m_program = std::make_unique<QOpenGLShaderProgram>();
	m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
	m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, "frag.glsl");
	m_program->bindAttributeLocation("texCoord", 0);
	m_program->link();
	m_program->bind();

	// Create a vertex array object. In OpenGL ES 2.0 and OpenGL 2.x
	// implementations this is optional and support may not be present
	// at all. Nonetheless the below code works in all cases and makes
	// sure there is a VAO when one is needed.
	m_vao.create();
	QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);

	initBuffers();
	initTexture();
	fps.start();

	m_program->release();
}

void GlMatrixDraw::initBuffers()
{
	const size_t DATA_SIZE = dataCount() * conf.bytes_per_val;
	for (auto& el : input_qs)
	{
		glGenBuffers(1, &el.pbo_id);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, el.pbo_id);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, DATA_SIZE, 0, GL_STREAM_DRAW);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void GlMatrixDraw::draw()
{
	++n_paint;
	std::swap(copy_idx, upload_idx);

	auto call_with_timer = [this](double& accum, auto fn) {
		timer.start();
		fn();
		double elapsed = timer.elapsed() * 1e-3;
		accum = (time_cnt > 0) ? 0.9 * accum + 0.1 * elapsed : elapsed;
	};
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	/* PBO stuff start */

	call_with_timer(copy_time, [this] { copy_frontbuffer_to_texture(); });
	call_with_timer(upload_time, [this] { process_upload_queue(); });

	/* PBO stuff end */

	// it is good idea to release PBOs with ID 0 after use.
	// Once bound with 0, all pixel operations behave normal ways.
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	QOpenGLVertexArrayObject::Binder vaoBinder(&m_vao);
	m_program->bind();
	m_program->setUniformValue("tex", 0);
	m_program->setUniformValue("is_radar_plot", static_cast<GLint>(is_radar_plot));

	glBindTexture(GL_TEXTURE_2D, textureId);
	glDrawArrays(GL_TRIANGLES, 0,
			3);                      // 3, since we draw a single full screen triangle
	glBindTexture(GL_TEXTURE_2D, 0); // unbind

	m_program->release();
	++time_cnt;
	if (time_cnt % 1000 == 0)
	{
		float ms = fps.elapsed() * 1e-3;
		std::cout << "Copy time: " << copy_time << "s\n"
				  << "Upload time: " << upload_time << "s\n"
				  << "FPS: " << n_paint / ms << "\n";
		fps.start();
		n_paint = 0;
	}
}

void GlMatrixDraw::initTexture()
{
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, conf.internal_format, tex_width, tex_height, 0, conf.pixel_format, conf.pixel_type, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void GlMatrixDraw::copy_frontbuffer_to_texture()
{
	// bind the texture and PBO
	glBindTexture(GL_TEXTURE_2D, textureId);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, input_qs[copy_idx].pbo_id);

	// copy pixels from PBO to texture object
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height, conf.pixel_format, conf.pixel_type, 0);
}

void GlMatrixDraw::process_upload_queue()
{
	UploadQueue& active_queue = input_qs[upload_idx];

	/* Basic idea:
	 *
	 * - Find contiguous matrix rows for upload.
	 * - previous row, if the currend row is the same.
	 * - If there is a gap or we reached the end of the matrix, upload what we have so far.
	 * - Try again, until we reached the end of the matrix
	 *
	 */

	int current_idx = 0;
	while (active_queue.input_q.size_approx() > 0 && active_queue.input_q.peek()->matrix_row >= current_idx)
	{
		UploadJob job;
		job.start_row_idx = active_queue.input_q.peek()->matrix_row;
		job.current_row_idx = -1;

		typename UploadQueue::Entry e;
		current_idx = job.start_row_idx;
		while (current_idx < tex_height && active_queue.input_q.size_approx() > 0 &&
				active_queue.input_q.peek()->matrix_row >= current_idx && active_queue.input_q.try_dequeue(e))
		{
			assert(e.values->size() >= conf.n_primitives_per_val * static_cast<size_t>(tex_width));
			assert(e.values->size() % conf.n_primitives_per_val == 0);

			if (job.current_row_idx == current_idx)
			{
				// previous insert was for this row, too; skip it
				job.data.pop_back();
			}
			job.data.emplace_back(e.values);
			job.current_row_idx = current_idx;
			++current_idx;
		}

		if (job.data.size() > 0)
		{
			upload_to_pbo(active_queue.pbo_id, job.start_row_idx, job.data);
		}
	} // while data in queue
}

bool GlMatrixDraw::upload_to_pbo(GLuint pbo_id, int start_row_idx, const std::vector<RowPtr>& data)
{
	const size_t row_bytes = tex_width * conf.bytes_per_val;
	const size_t start_byte_offset = start_row_idx * row_bytes;
	const size_t upload_size = data.size() * row_bytes;

	// bind PBO to update pixel values
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo_id);

	// map the buffer object into client's memory
	// note: using char* here, to get one byte increments when adding pointers below
	auto* ptr = (char*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, start_byte_offset, upload_size, GL_MAP_WRITE_BIT);
	if (ptr)
	{
		for (size_t i = 0; i < data.size(); i++)
		{
			const Row& row = *data[i];
			assert(row.size() >= static_cast<size_t>(tex_width));
			memcpy(ptr + i * tex_width * conf.bytes_per_val, row.data(), row_bytes);
		}
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release pointer to mapping buffer
		return true;
	}
	else
	{
		std::cerr << "Mapping buffer didn't work :/\n";
		return false;
	}
}

void GlMatrixDraw::append(std::vector<float> input)
{
	insert(append_pos, input);
	append_pos = (append_pos + 1) % tex_height;
}
