/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XL2dIcons.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

static void drawIcon_Dynamic_Loader(vg::VectorImage &image, float pr) {
	float p = pr;

	float arcLen = 20.0_to_rad;
	float arcStart = -100.0_to_rad;
	if (p < 0.5) {
		arcStart = arcStart + progress(0_to_rad, 75_to_rad, p * 2.0f);
		arcLen = progress(20_to_rad, 230_to_rad, p * 2.0f);
	} else {
		arcStart = arcStart + progress(75_to_rad, 360_to_rad, (p - 0.5f) * 2.0f);
		arcLen = progress(230_to_rad, 20_to_rad, (p - 0.5f) * 2.0f);
	}

	image.addPath("")
			->openForWriting([&](vg::PathWriter &writer) {
		writer.addArc(Rect(4, 4, 16, 16), arcStart, arcLen);
	})
			.setStyle(vg::DrawFlags::Stroke)
			.setStrokeWidth(2.0f);
}

static void drawIcon_Dynamic_Nav(vg::VectorImage &image, float pr) {
	auto t = Mat4::IDENTITY;
	t.translate(12, 12, 0);

	float p = pr;

	if (p <= 1.0f) {
		t.rotateZ(pr * numbers::pi);
		t.translate(-12, -12, 0);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(progress(2.0f, 13.0f, p), progress(5.0f, 3.0f, p))
					.lineTo(progress(2.0f, 13.0f - float(M_SQRT2), p),
							progress(7.0f, 3.0f + float(M_SQRT2), p))
					.lineTo(progress(22.0f, 22.0f - float(M_SQRT2), p),
							progress(7.0f, 12.0f + float(M_SQRT2), p))
					.lineTo(progress(22.0f, 22.0f, p), progress(5.0f, 12.0f, p))
					.closePath();
		}).setTransform(t);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(progress(2.0f, 3.0f, p), 11)
					.lineTo(progress(22.0f, 20.0f, p), 11)
					.lineTo(progress(22.0f, 20.0f, p), 13)
					.lineTo(progress(2.0f, 3.0f, p), 13)
					.closePath();
		}).setTransform(t);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(progress(2.0f, 13.0f - float(M_SQRT2), p),
						  progress(17.0f, 21.0f - float(M_SQRT2), p))
					.lineTo(progress(22.0f, 22.0f - float(M_SQRT2), p),
							progress(17.0f, 12.0f - float(M_SQRT2), p))
					.lineTo(progress(22.0f, 22.0f, p), progress(19.0f, 12.0f, p))
					.lineTo(progress(2.0f, 13.0f, p), progress(19.0f, 21.0f, p))
					.closePath();
		}).setTransform(t);
	} else {
		p = p - 1.0f;

		t.rotateZ((1.0f + 0.25f * p) * numbers::pi);
		t.translate(-12, -12, 0);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(13.0f, progress(3.0f, 4.0f, p))
					.lineTo(progress(13.0f - float(M_SQRT2), 11.0f, p),
							progress(3.0f + float(M_SQRT2), 4.0f, p))
					.lineTo(progress(22.0f - float(M_SQRT2), 11.0f, p),
							progress(12.0f + float(M_SQRT2), 12.0f, p))
					.lineTo(progress(22.0f, 13.0f, p), 12.0f)
					.closePath();
		}).setTransform(t);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(progress(3.0f, 4.0f, p), 11)
					.lineTo(progress(20.0f, 20.0f, p), 11)
					.lineTo(progress(20.0f, 20.0f, p), 13)
					.lineTo(progress(3.0f, 4.0f, p), 13)
					.closePath();
		}).setTransform(t);

		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.moveTo(progress(13.0f - float(M_SQRT2), 11.0f, p),
						  progress(21.0f - float(M_SQRT2), 20.0f, p))
					.lineTo(progress(22.0f - float(M_SQRT2), 11.0f, p),
							progress(12.0f - float(M_SQRT2), 12.0f, p))
					.lineTo(progress(22.0f, 13.0f, p), 12.0f)
					.lineTo(13.0f, progress(21.0f, 20.0f, p))
					.closePath();
		}).setTransform(t);
	}
}

static void drawIcon_Dynamic_DownloadProgress(vg::VectorImage &image, float pr) {
	auto t = Mat4::IDENTITY;
	t.scale(-1, 1, 1);
	t.translate(-24, 0, 0);
	if (pr >= 1.0f) {
		image.addPath("")
				->openForWriting(
						[&](vg::PathWriter &writer) { writer.addOval(Rect(3, 3, 18, 18)); })
				.setStyle(vg::DrawFlags::Stroke)
				.setStrokeWidth(2.0f);
	} else if (pr <= 0.0f) {
		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.addArc(Rect(3, 3, 18, 18), 90.0_to_rad, 1_to_rad);
		})
				.setStyle(vg::DrawFlags::Stroke)
				.setStrokeWidth(2.0f);
	} else {
		image.addPath("")
				->openForWriting([&](vg::PathWriter &writer) {
			writer.addArc(Rect(3, 3, 18, 18), 90.0_to_rad, 360_to_rad * pr);
		})
				.setStyle(vg::DrawFlags::Stroke)
				.setStrokeWidth(2.0f)
				.setTransform(t);
	}

	image.addPath("")->openForWriting(
			[](vg::PathWriter &writer) { writer.addRect(Rect(9, 9, 6, 6)); });
}

static void drawIcon_Stappler_CursorIcon(vg::VectorImage &image, float pr) {
	image.addPath()->openForWriting([](vg::PathWriter &writer) {
		writer.moveTo(12.0f, 24.0f)
				.lineTo(5.0f, 17.0f)
				.arcTo(7.0f * sqrt(2.0f), 7.0f * sqrt(2.0f), 0.0f, true, true, 19.0f, 17.0f)
				.closePath();
	});
}

static void drawIcon_Stappler_SelectioinStartIcon(vg::VectorImage &image, float pr) {
	image.addPath()->openForWriting([](vg::PathWriter &writer) {
		writer.moveTo(24, 24).lineTo(12, 24).arcTo(12, 12, 0, true, true, 24, 12).closePath();
	});
}

static void drawIcon_Stappler_SelectioinEndIcon(vg::VectorImage &image, float pr) {
	image.addPath()->openForWriting([](vg::PathWriter &writer) {
		writer.moveTo(0, 24).lineTo(0, 12).arcTo(12, 12, 0, true, true, 12, 24).closePath();
	});
}

void drawIcon(vg::VectorImage &image, IconName name, float pr) {
	switch (name) {
	case IconName::None:
	case IconName::Empty: break;
	case IconName::Stappler_CursorIcon: drawIcon_Stappler_CursorIcon(image, pr); break;
	case IconName::Stappler_SelectioinStartIcon:
		drawIcon_Stappler_SelectioinStartIcon(image, pr);
		break;
	case IconName::Stappler_SelectioinEndIcon:
		drawIcon_Stappler_SelectioinEndIcon(image, pr);
		break;
	case IconName::Dynamic_Loader: drawIcon_Dynamic_Loader(image, pr); break;
	case IconName::Dynamic_Nav: drawIcon_Dynamic_Nav(image, pr); break;
	case IconName::Dynamic_DownloadProgress: drawIcon_Dynamic_DownloadProgress(image, pr); break;
	default:
		getIconData(name, [&](BytesView bytes) {
			auto t = Mat4::IDENTITY;
			t.scale(1, -1, 1);
			t.translate(0, -24, 0);
			// adding icon to tesselator cache with name org.stappler.xenolith.icon.*
			auto path = image.addPath("",
									 toString("org.stappler.xenolith.icon.",
											 xenolith::basic2d::getIconName(name)))
								->getPath();
			path->init(bytes);
			path->setTransform(t);
		});
	}
}

} // namespace stappler::xenolith::basic2d
