#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLBuffer>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtensions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QTime>

#include <QtGui/QImage>
#include <array>
#include <iostream>
#include <memory>

#include <lockfree_q/readerwriterqueue.h>

QT_FORWARD_DECLARE_CLASS(QOpenGLShaderProgram)

class GLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
	Q_OBJECT

public:
	using T = float;
	using Row = std::vector<T>;
	using RowPtr = std::shared_ptr<Row>;

	GLWidget(size_t rows, size_t cols, QWidget* parent = 0);
	~GLWidget();

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

	size_t dataCount() const { return tex_width * tex_height; }

	void append(Row input)
	{
		insert(append_pos, input);
		append_pos = (append_pos + 1) % tex_height;
	}

	bool insert(int pos, Row input)
	{
		if (pos >= tex_height)
		{
			return false;
		}
		assert(input.size() == tex_width);
		auto input_ptr = std::make_shared<Row>(std::move(input));

		input_qs[0].input_q.enqueue(UploadQueue::Entry{append_pos, input_ptr});
		input_qs[1].input_q.enqueue(UploadQueue::Entry{append_pos, input_ptr});
		return true;
	}

public slots:
	void issue_redraw() { update(); };
	void set_is_radarplot(int state) { is_radar_plot = state != 0; }

	void cleanup();
	static void openGLErrorRecieved(const QOpenGLDebugMessage& debugMessage);

protected:
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int width, int height) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

	void initBuffers();
	void initTexture();

private:
	void copy_frontbuffer_to_texture();
	void process_upload_queue();
	bool upload_to_pbo(GLuint pbo_id, int start_row_idx, const std::vector<RowPtr>& data);

	int tex_width;
	int tex_height;

	QOpenGLVertexArrayObject m_vao;
	std::unique_ptr<QOpenGLShaderProgram> m_program;

	GLuint textureId; // ID of texture

	GLuint upload_idx;
	GLuint copy_idx;

	std::unique_ptr<QOpenGLDebugLogger> logger;

	QTime timer;
	QTime fps;

	double upload_time;
	double copy_time;
	long time_cnt;

	bool is_radar_plot;

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
	std::vector<T> upload_prepare_buffer;

	long n_paint;
	int append_pos; /// last append position
};

#endif
