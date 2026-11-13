#pragma once

#include "ProductRecord.h"
#include "Recipe.h"

class FinalJudge
{
public:
    void setRecipe(const Recipe &recipe);
    const Recipe &recipe() const { return m_recipe; }

    /** Apply recipe tolerances; fills judged/pass/failCode/failReason. */
    void judge(ProductRecord *rec) const;

private:
    Recipe m_recipe;
};
