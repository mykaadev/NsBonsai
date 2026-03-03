#include "NsBonsaiSettings.h"

UNsBonsaiSettings::UNsBonsaiSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Ns Bonsai");
}

FName UNsBonsaiSettings::GetCategoryName() const
{
	return CategoryName;
}
