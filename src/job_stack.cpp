// SPDX-License-Identifier: GPL-3.0-or-later

#include "job_stack.hpp"

#include <algorithm>

namespace inkcut {

int computeStackCountX(const MaterialSettings& mat, const QRectF& copy_bbox, double spacing_x,
                       double spacing_y)
{
    const double avail[2] = {mat.width - mat.padding_left - mat.padding_right,
                             mat.height - mat.padding_top - mat.padding_bottom};
    const double size[2] = {std::max(0.01, copy_bbox.width()), std::max(0.01, copy_bbox.height())};
    const double spacing[2] = {spacing_x, spacing_y};
    int stack_x = 0;
    double p[2] = {0, 0};
    for (int i = 0; i < 2; ++i) {
        while ((p[i] + size[i]) < avail[i]) {
            if (i == 0)
                ++stack_x;
            p[i] += size[i] + spacing[i];
        }
    }
    return stack_x;
}

void applyAddStack(int& copies, int stack_x)
{
    if (stack_x <= 0) {
        copies += 1;
        return;
    }
    const int rem = copies % stack_x;
    if (rem == 0)
        copies += stack_x;
    else
        copies += stack_x - rem;
}

void applyRemoveStack(int& copies, int stack_x)
{
    if (stack_x <= 0 || copies <= stack_x) {
        copies = 1;
        return;
    }
    const int rem = copies % stack_x;
    if (rem == 0)
        copies -= stack_x;
    else
        copies -= rem;
}

} // namespace inkcut
