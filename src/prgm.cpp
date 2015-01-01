#include "prgm.h"

CodeUnit *Program::get_item_by_name(ProgIdent ident) {

}

void Program::add_item(Item &item) {
    struct AddItemVisitor : public ItemVisitor {
        AddItemVisitor(Program *prgm) : prgm(prgm) {};
        Program *prgm;

        virtual void visit(FunctionItem *item) {
            // Get the item
            prgm->get_item_by_name(ProgIdent(item->proto.name));
        };
        virtual void visit(StructItem *item) {
        };
        virtual void visit(FFIFunctionItem *item) {
            // TODO: Implement
        };
        virtual void visit(EmptyItem *) { /* pass */ };
    };

    AddItemVisitor visitor(this);
    item.accept(visitor);
}

void Program::add_items(std::vector<std::unique_ptr<Item>> &items) {
    for (auto &item : items) {
        add_item(*item);
    }
}
