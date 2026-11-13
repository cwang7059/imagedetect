#include "FinalJudge.h"

#include <QtMath>

void FinalJudge::setRecipe(const Recipe &recipe)
{
    m_recipe = recipe;
}

void FinalJudge::judge(ProductRecord *rec) const
{
    if (!rec)
        return;

    rec->judged = true;
    rec->pass = false;
    rec->failCode = FailCode::None;
    rec->failReason.clear();

    if (!rec->od.ready || !rec->length.ready || !rec->defect.ready) {
        rec->failCode = FailCode::TimeoutMissing;
        rec->failReason = QStringLiteral("Missing station result");
        return;
    }
    if (!rec->od.algoOk || !rec->length.algoOk || !rec->defect.algoOk) {
        rec->failCode = FailCode::AlgoError;
        rec->failReason = QStringLiteral("Algorithm failure on a station");
        return;
    }

    const double dTol = m_recipe.dTolMm;
    if (qAbs(rec->od.d1Mm - m_recipe.dNomMm) > dTol
        || qAbs(rec->od.d2Mm - m_recipe.dNomMm) > dTol) {
        rec->failCode = FailCode::OdOutOfTol;
        rec->failReason = QStringLiteral("OD out of tolerance D1=%1 D2=%2")
                              .arg(rec->od.d1Mm, 0, 'f', 3)
                              .arg(rec->od.d2Mm, 0, 'f', 3);
        return;
    }

    if (qAbs(rec->length.lengthMm - m_recipe.lNomMm) > m_recipe.lTolMm) {
        rec->failCode = FailCode::LengthOutOfTol;
        rec->failReason = QStringLiteral("Length out of tolerance L=%1")
                              .arg(rec->length.lengthMm, 0, 'f', 3);
        return;
    }

    if (rec->defect.grade > m_recipe.maxDefectGrade) {
        rec->failCode = FailCode::Defect;
        rec->failReason = QStringLiteral("Defect grade %1 > max %2")
                              .arg(rec->defect.grade)
                              .arg(m_recipe.maxDefectGrade);
        return;
    }

    rec->pass = true;
    rec->failCode = FailCode::None;
    rec->failReason = QStringLiteral("OK");
}
