#include "addItemCommand.h"

using namespace qReal::commands;
using namespace qReal::shapeEdit;

AddItemCommand::AddItemCommand(QGraphicsScene *scene, QGraphicsItem *item)
    : mScene(scene)
    , mItem(item)
    , mIsExecuted(false)
{}

AddItemCommand::~AddItemCommand()
{
    if (!mIsExecuted && !mItem->parentObject()) {
        // Item must be deleted because
        // in that case no owner of that item exist.
        delete mItem;
    }
}

bool AddItemCommand::execute()
{
    if (mItem && !mIsExecuted) {
        mScene->addItem(mItem);
        mIsExecuted = true;
        return true;
    }
    return false;
}

bool AddItemCommand::restoreState()
{
    if (mItem && mIsExecuted) {
        mScene->removeItem(mItem);
        mIsExecuted = false;
        return true;
    }
    return false;
}
