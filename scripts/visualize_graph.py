import pygame
import re
import sys

ZOOM = 0.75
# subs a certain amount off of the right and bottom of char quads so I can see the borders
RIGHT_SUB = 1
BOTTOM_SUB = 0

# Initialize pygame
pygame.init()

# Set up display
screen_width = 1200
screen_height = 800
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Graph Visualization")


def str_pos_to_coord(coord: str) -> int:
    return int(int(coord) * ZOOM)


# Function to parse the input file
def parse_graph_layout(filename):
    graph_info = {'width': 0, 'height': 0, 'ranksep': 0, 'nodesep': 0}
    ranks = []
    nodes = []
    edges = []

    with open(filename, 'r') as f:
        lines = f.readlines()

    current_node = None
    graph_pattern = re.compile(r'Graph\((\d+), (\d+)\)')
    sep_pattern = re.compile(r'RankSep=(\d+) NodeSep=(\d+)')
    rank_pattern = re.compile(r'Rank (\d+)\((\d+), (\d+), (\d+), (\d+)\)')
    node_pattern = re.compile(r'Node (\w+)\((\d+), (\d+), (\d+), (\d+)\)')
    quad_pattern = re.compile(r'Quad\((\d+), (\d+), (\d+), (\d+)\)')
    edge_pattern = re.compile(r'Edge (\w+) -> (\w+)')

    for line in lines:
        line = line.strip()
        graph_match = graph_pattern.match(line)
        sep_match = sep_pattern.match(line)
        rank_match = rank_pattern.match(line)
        node_match = node_pattern.match(line)
        quad_match = quad_pattern.match(line)
        edge_match = edge_pattern.match(line)

        if graph_match:
            # Extract graph dimensions
            width, height = map(str_pos_to_coord, graph_match.groups())
            graph_info['width'] = width
            graph_info['height'] = height
        elif sep_match:
            # Extract rank and node separation
            ranksep, nodesep = map(str_pos_to_coord, sep_match.groups())
            graph_info['ranksep'] = ranksep
            graph_info['nodesep'] = nodesep
        elif rank_match:
            # Extract rank information
            rank_id, x, y, width, height = map(str_pos_to_coord, rank_match.groups())
            ranks.append({
                'id': rank_id,
                'x': x,
                'y': y,
                'width': width,
                'height': height
            })
        elif node_match:
            # Extract node bounds
            name = node_match.group(1)
            left, top, right, bottom = map(str_pos_to_coord, node_match.groups()[1:])
            current_node = {'name': name, 'bounds': (left, top, right, bottom), 'quads': []}
            nodes.append(current_node)
        elif quad_match and current_node:
            # Extract quad bounds
            left, top, right, bottom = map(str_pos_to_coord, quad_match.groups())
            current_node['quads'].append((left, top, right - RIGHT_SUB, bottom - BOTTOM_SUB))
        elif edge_match:
            # Extract edge information
            source, target = edge_match.groups()
            edges.append((source, target))

    return graph_info, ranks, nodes, edges


# Main function to visualize the graph layout
def main():
    if len(sys.argv) != 2:
        raise RuntimeError("Expected exactly one CLI arg: filepath")
    filepath = sys.argv[1]
    graph_info, ranks, nodes, edges = parse_graph_layout(filepath)

    # Determine the bounding box of all elements to center the view
    min_x = min(node['bounds'][0] for node in nodes) if nodes else 0
    min_y = min(node['bounds'][1] for node in nodes) if nodes else 0
    max_x = max(node['bounds'][2] for node in nodes) if nodes else graph_info['width']
    max_y = max(node['bounds'][3] for node in nodes) if nodes else graph_info['height']

    # Calculate offsets to center the graph
    # x_offset = (screen_width - (max_x - min_x)) // 2 - min_x
    # y_offset = (screen_height - (max_y - min_y)) // 2 - min_y
    x_offset = 50
    y_offset = 50

    running = True
    while running:
        screen.fill((255, 255, 255))  # Fill the screen with white

        # Draw rank areas (optional, for visualization)
        for rank in ranks:
            pygame.draw.rect(screen, (240, 240, 255),
                             (rank['x'] + x_offset, rank['y'] + y_offset,
                              rank['width'], rank['height']), 0)
            pygame.draw.rect(screen, (200, 200, 230),
                             (rank['x'] + x_offset, rank['y'] + y_offset,
                              rank['width'], rank['height']), 2)

        # Draw nodes
        for node in nodes:
            node_bounds = node['bounds']
            node_left, node_top, node_right, node_bottom = node_bounds
            node_width = node_right - node_left
            node_height = node_bottom - node_top

            # Draw the node bounding box in black
            pygame.draw.rect(screen, (0, 0, 0),
                             (node_left + x_offset, node_top + y_offset,
                              node_width, node_height), 2)

            # Draw node name
            font = pygame.font.SysFont('Arial', 10)
            name_surf = font.render(node['name'], True, (255, 0, 0))
            screen.blit(name_surf, (node_left + x_offset, node_top + y_offset - 15))

            for quad in node['quads']:
                quad_left, quad_top, quad_right, quad_bottom = quad
                quad_width = quad_right - quad_left
                quad_height = quad_bottom - quad_top

                # Draw the filled black quad inside the node
                pygame.draw.rect(screen, (0, 0, 0),
                                 (quad_left + x_offset, quad_top + y_offset,
                                  quad_width, quad_height))

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
