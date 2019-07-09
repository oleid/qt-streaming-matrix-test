#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QOpenGLDebugLogger>
#include <QOpenGLWidget>

#include "glmatrixdraw.h"

class GLWidget : public QOpenGLWidget, public GlMatrixDraw
{
	Q_OBJECT

public:
	GLWidget(size_t rows, size_t cols, detail::texConf config, QWidget* parent = 0)
		: QOpenGLWidget(parent)
		, GlMatrixDraw(rows, cols, config)
	{
	}

	~GLWidget() { cleanup(); }

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

	void initializeGL() override
	{
		connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &GLWidget::cleanup);
		GlMatrixDraw::initializeGL();

		if (context()->hasExtension(QByteArrayLiteral("GL_KHR_debug")))
		{
			qDebug() << "Logging OpenGL errors\n";
			logger = std::make_unique<QOpenGLDebugLogger>(this);
			logger->initialize();
			connect(logger.get(), &QOpenGLDebugLogger::messageLogged, this, &openGLErrorRecieved);
		}
	}
public slots:
	void issue_redraw() { update(); };
	void set_is_radarplot(int state) { is_radar_plot = state != 0; }

	void cleanup();

protected:
	void paintGL() override;
	void resizeGL(int width, int height) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

	static void openGLErrorRecieved(const QOpenGLDebugMessage& debugMessage);
	std::unique_ptr<QOpenGLDebugLogger> logger;
};

#endif
