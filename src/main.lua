Menu = {
    items = {
        { label = "Host", action = "host" },
        { label = "Connect", action = "connect" }
    },
    selected = 1
}

function Menu:get_labels()
    local labels = {}
    for i, item in ipairs(self.items) do
        table.insert(labels, (i == self.selected and "> " or "  ") .. item.label)
    end
    return labels
end

function Menu:handle_input(key)
    if key == "up" then
        self.selected = self.selected > 1 and self.selected - 1 or #self.items
    elseif key == "down" then
        self.selected = self.selected < #self.items and self.selected + 1 or 1
    elseif key == "return" then
        return self.items[self.selected].action
    end
    return nil
end
