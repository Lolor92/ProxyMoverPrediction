#include "Data/SGM_ReactionData.h"

bool USGM_ReactionData::FindReaction(FGameplayTag ReactionTag, FSGM_ReactionDataEntry& OutReaction) const
{
	const FSGM_ReactionDataEntry* Reaction = FindReactionPtr(ReactionTag);
	if (!Reaction) return false;

	OutReaction = *Reaction;
	return true;
}

const FSGM_ReactionDataEntry* USGM_ReactionData::FindReactionPtr(FGameplayTag ReactionTag) const
{
	if (!ReactionTag.IsValid()) return nullptr;

	for (const FSGM_ReactionDataEntry& Entry : Reactions)
	{
		// MatchesTag allows Reaction.Pushback.Heavy to use a generic Reaction.Pushback entry if needed.
		if (Entry.ReactionTag.IsValid() && ReactionTag.MatchesTag(Entry.ReactionTag))
		{
			return &Entry;
		}
	}

	return nullptr;
}
