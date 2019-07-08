/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "window.h"
#include "glwidget.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <random>

#if defined(_POSIX_VERSION)
#include <pthread.h>
#endif

Window::Window(MainWindow* mw)
	: mainWindow(mw)
{
	glWidget = new GLWidget(360, 32768);

	xSlider = createSlider();
	ySlider = createSlider();
	zSlider = createSlider();

	/*	connect(xSlider, &QSlider::valueChanged, glWidget,
	   &GLWidget::setXRotation); connect(glWidget, &GLWidget::xRotationChanged,
	   xSlider, &QSlider::setValue); connect(ySlider, &QSlider::valueChanged,
	   glWidget, &GLWidget::setYRotation); connect(glWidget,
	   &GLWidget::yRotationChanged, ySlider, &QSlider::setValue);
		connect(zSlider, &QSlider::valueChanged, glWidget,
	   &GLWidget::setZRotation); connect(glWidget, &GLWidget::zRotationChanged,
	   zSlider, &QSlider::setValue);
	*/
	QVBoxLayout* mainLayout = new QVBoxLayout;
	QHBoxLayout* container = new QHBoxLayout;
	container->addWidget(glWidget);
	container->addWidget(xSlider);
	container->addWidget(ySlider);
	container->addWidget(zSlider);

	QCheckBox* is_radarplot = new QCheckBox;
	is_radarplot->setTristate(false);
	is_radarplot->setText("Radarplot");

	container->addWidget(is_radarplot);
	connect(is_radarplot, &QCheckBox::stateChanged, glWidget, &GLWidget::set_is_radarplot);

	QWidget* w = new QWidget;
	w->setLayout(container);
	mainLayout->addWidget(w);
	dockBtn = new QPushButton(tr("Undock"), this);
	connect(dockBtn, &QPushButton::clicked, this, &Window::dockUndock);
	mainLayout->addWidget(dockBtn);

	setLayout(mainLayout);

	xSlider->setValue(15 * 16);
	ySlider->setValue(345 * 16);
	zSlider->setValue(0 * 16);

	setWindowTitle(tr("Hello GL"));

	redraw_timer = new QTimer();
	redraw_timer->start(17);

	connect(redraw_timer, &QTimer::timeout, glWidget, &GLWidget::issue_redraw);

	data_gen = std::make_unique<std::thread>(MockDataSource(glWidget));
	data_gen->detach();
}

QSlider* Window::createSlider()
{
	QSlider* slider = new QSlider(Qt::Vertical);
	slider->setRange(0, 360 * 16);
	slider->setSingleStep(16);
	slider->setPageStep(15 * 16);
	slider->setTickInterval(15 * 16);
	slider->setTickPosition(QSlider::TicksRight);
	return slider;
}

void Window::keyPressEvent(QKeyEvent* e)
{
	if (e->key() == Qt::Key_Escape)
		close();
	else
		QWidget::keyPressEvent(e);
}

void Window::dockUndock()
{
	if (parent())
	{
		setParent(0);
		setAttribute(Qt::WA_DeleteOnClose);
		move(QApplication::desktop()->width() / 2 - width() / 2,
				QApplication::desktop()->height() / 2 - height() / 2);
		dockBtn->setText(tr("Dock"));
		show();
	}
	else
	{
		if (!mainWindow->centralWidget())
		{
			if (mainWindow->isVisible())
			{
				setAttribute(Qt::WA_DeleteOnClose, false);
				dockBtn->setText(tr("Undock"));
				mainWindow->setCentralWidget(this);
			}
			else
			{
				QMessageBox::information(0, tr("Cannot dock"), tr("Main window already closed"));
			}
		}
		else
		{
			QMessageBox::information(0, tr("Cannot dock"), tr("Main window already occupied"));
		}
	}
}

void Window::genData() {}

void Window::MockDataSource::operator()()
{
	using namespace std::chrono_literals;
	using namespace std::chrono;
	using T = std::complex<float>;

#if defined(_POSIX_VERSION)
	pthread_setname_np(pthread_self(), "DataGen");
#endif

	const auto target_dps = 1200;
	const auto max_sleep_time = nanoseconds(1000000000 / target_dps);
	const int N = 32768;
	const float scaling = 10.0f / N;

	std::default_random_engine generator;
	std::uniform_real_distribution<float> distribution(-1.0, 1.0);
	float pos = static_cast<float>(N) / 2.0;
	while (true)
	{
		auto start = steady_clock::now();
		pos += 1e-3 * N * distribution(generator);

		std::vector<T> v(N, 0.0f);
		for (int i = std::max(0, int(pos - 60)); i < std::min(N, int(pos + 60)); i++)
		{
			const auto intensity = exp(-(i - pos) * (i - pos) * scaling);
			v[i] = (std::abs(intensity) > 1e-4) ? intensity : T(0);
		}
		w->append(v);

		if (++n_count % 10000 == 0)
		{
			float ms = dps.elapsed() * 1e-3;
			if (ms > 0)
			{
				auto dps = 10000 / ms;
				std::cerr << "DPS: " << dps << "\n";
			}
			n_count = 0;
			dps.start();
		}

		auto end = steady_clock::now();
		auto append_time = duration_cast<nanoseconds>(end - start);
		auto sleep_time = max_sleep_time - append_time;

		std::this_thread::sleep_for(sleep_time);
	}
}
