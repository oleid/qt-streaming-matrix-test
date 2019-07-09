//
// Created by Olaf Leidinger <oleid@mescharet.de> on 07.07.19.
//

#ifndef GLMATRIXDRAW_H_uobsbvgw23
#define GLMATRIXDRAW_H_uobsbvgw23

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QTime>

#include <array>
#include <iostream>
#include <memory>

#include <complex>
#include <lockfree_q/readerwriterqueue.h>

namespace detail
{
struct texConf
{
	explicit texConf(float);
	explicit texConf(std::complex<float>);

	const GLuint bytes_per_val = sizeof(GLfloat);
	const GLuint n_primitives_per_val; // 1 for real, 2 for complex
	const GLuint internal_format;      // i.e. GL_R32F, GL_RG32F
	const GLuint pixel_format;         // i.e. GL_RED, GL_RG
	const GLuint pixel_type;           // i.e. GL_FLOAT
};

} // namespace detail
QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

class GlMatrixDraw : protected QOpenGLExtraFunctions
{
public:
	using Row = std::vector<float>;
	using RowPtr = std::shared_ptr<Row>;

	GlMatrixDraw(size_t rows, size_t cols, detail::texConf config);
	~GlMatrixDraw();

	size_t dataCount() const { return tex_width * tex_height; }

	void append(std::vector<float> input);

	bool insert(int pos, std::vector<float> input);

protected:
	void initializeGL();
	void draw();

	void initBuffers();
	void initTexture();

	bool is_radar_plot;

	void cleanup();

private:
	Row maybe_scale_data(Row&& input) const;
	void copy_frontbuffer_to_texture();
	void process_upload_queue();
	bool upload_to_pbo(GLuint pbo_id, int start_row_idx, const std::vector<RowPtr>& data);

	int tex_width;
	int tex_height;
	int input_data_len;

	QOpenGLVertexArrayObject m_vao;
	std::unique_ptr<QOpenGLShaderProgram> m_program;

	GLuint textureId; // ID of texture

	GLuint upload_idx;
	GLuint copy_idx;

	QTime timer;
	QTime fps;

	double upload_time;
	double copy_time;
	long time_cnt;

	struct UploadQueue
	{
		struct Entry
		{
			int matrix_row;
			RowPtr values;
		};
		using LockFreeQueue = moodycamel::ReaderWriterQueue<Entry>;

		UploadQueue(int reserved_size)
			: pbo_id(0)
			, input_q(reserved_size)
		{
		}

		GLuint pbo_id;
		LockFreeQueue input_q;
	};

	std::array<UploadQueue, 2> input_qs;

	long n_paint;
	int append_pos; /// last append position
	detail::texConf conf;
};

#endif //GLMATRIXDRAW_H_uobsbvgw23
