import pygame
import re
import sys

ZOOM = 1.5
# subs a certain amount off of the right and bottom of char quads so I can see the borders
RIGHT_SUB = 1
BOTTOM_SUB = 0

# Initialize pygame
pygame.init()

# Set up display
screen_width = 1000
screen_height = 600
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Node Visualization")


def node_str_pos_to_coord(coord: str) -> int:
    return int(int(coord) * ZOOM)


# Function to parse the input file
def parse_nodes(filename):
    nodes = []
    with open(filename, 'r') as f:
        lines = f.readlines()

    current_node = None
    node_pattern = re.compile(r'Node \w+\((\d+), (\d+), (\d+), (\d+)\)')
    quad_pattern = re.compile(r'Quad\((\d+), (\d+), (\d+), (\d+)\)')

    for line in lines:
        line = line.strip()
        node_match = node_pattern.match(line)
        quad_match = quad_pattern.match(line)

        if node_match:
            # Extract node bounds
            left, top, right, bottom = map(node_str_pos_to_coord, node_match.groups())
            current_node = {'bounds': (left, top, right, bottom), 'quads': []}
            nodes.append(current_node)
        elif quad_match and current_node:
            # Extract quad bounds
            left, top, right, bottom = map(node_str_pos_to_coord, quad_match.groups())
            current_node['quads'].append((left, top, right - RIGHT_SUB, bottom - BOTTOM_SUB))

    return nodes


# Main function to visualize the nodes and quads
def main():
    if len(sys.argv) != 2:
        raise RuntimeError("Expected exactly one CLI arg: filepath")
    filepath = sys.argv[1]
    nodes = parse_nodes(filepath)
    x_offset = 10  # Initial offset for placing nodes
    y_offset = 50  # Keep some margin at the top
    spacing = 20  # Space between nodes

    running = True
    while running:
        screen.fill((255, 255, 255))  # Fill the screen with white
        current_x = x_offset
        current_y = y_offset

        for node in nodes:
            node_bounds = node['bounds']
            node_width = node_bounds[2] - node_bounds[0]
            node_height = node_bounds[3] - node_bounds[1]

            # Draw the node bounding box in black
            pygame.draw.rect(screen, (0, 0, 0), (current_x, current_y, node_width, node_height), 2)

            for quad in node['quads']:
                quad_left = current_x + (quad[0] - node_bounds[0])
                quad_top = current_y + (quad[1] - node_bounds[1])
                quad_width = quad[2] - quad[0]
                quad_height = quad[3] - quad[1]

                # Draw the filled black quad inside the node
                pygame.draw.rect(screen, (0, 0, 0), (quad_left, quad_top, quad_width, quad_height))

            # Move to the next node position
            # current_x += node_width + spacing
            current_y += node_height + spacing

        # Event handling
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        # Update the display
        pygame.display.flip()

    pygame.quit()


# Run the program
if __name__ == "__main__":
    main()
