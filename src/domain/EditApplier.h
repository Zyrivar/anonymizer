// domain/EditApplier.h — применение списка замен к тексту.
// Чистая доменная логика: сортирует правки по убыванию позиции и применяет их,
// чтобы ранние замены не сдвигали байтовые смещения поздних.
#pragma once
#include "Types.h"
#include <string>
#include <vector>
#include <algorithm>

namespace domain {

// Одна правка: заменить диапазон [hit.start, hit.end) на replacement.
struct Edit {
    Hit         hit;
    std::string replacement;
};

class EditApplier {
public:
    // Применить правки к исходнику. Правки сортируются по убыванию start,
    // поэтому применяются справа налево — смещения остаются валидными.
    static std::string apply(const std::string& source, std::vector<Edit> edits) {
        std::sort(edits.begin(), edits.end(),
                  [](const Edit& a, const Edit& b) {
                      return a.hit.start > b.hit.start;
                  });
        std::string out = source;
        for (const auto& e : edits)
            out.replace(e.hit.start, e.hit.length(), e.replacement);
        return out;
    }
};

} // namespace domain
