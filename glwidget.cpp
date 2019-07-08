#include "glwidget.h"

QSize GLWidget::minimumSizeHint() const { return QSize(50, 50); }

QSize GLWidget::sizeHint() const { return QSize(400, 400); }

void GLWidget::cleanup()
{
	makeCurrent();

	GlMatrixDraw<Float>::cleanup();

	doneCurrent();
}

void GLWidget::paintGL() { draw(); }

void GLWidget::resizeGL(int, int) {}

void GLWidget::mousePressEvent(QMouseEvent*) {}

void GLWidget::mouseMoveEvent(QMouseEvent*) {}

void GLWidget::openGLErrorRecieved(const QOpenGLDebugMessage& debugMessage)
{
	qDebug() << debugMessage << "\n";
}
