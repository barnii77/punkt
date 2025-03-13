import pygame
import re
import sys

ZOOM = 0.75
RIGHT_SUB = 1
BOTTOM_SUB = 0

pygame.init()
screen_width = 1200
screen_height = 800
screen = pygame.display.set_mode((screen_width, screen_height))
pygame.display.set_caption("Graph Visualization")


def str_pos_to_coord(coord: str) -> int:
    return int(int(coord) * ZOOM)


def parse_graph_layout(filename):
    graph_info = {'width': 0, 'height': 0, 'ranksep': 0, 'nodesep': 0}
    ranks = []
    nodes = []
    edges = []  # Each edge will be a list of (x, y) tuples

    with open(filename, 'r') as f:
        lines = f.readlines()

    current_node = None
    graph_pattern = re.compile(r'Graph\((\d+), (\d+)\)')
    sep_pattern = re.compile(r'RankSep=(\d+) NodeSep=(\d+)')
    rank_pattern = re.compile(r'Rank (\d+)\((\d+), (\d+), (\d+), (\d+)\)')
    node_pattern = re.compile(r'Node (\w+)\((\d+), (\d+), (\d+), (\d+)\)')
    quad_pattern = re.compile(r'Quad\((\d+), (\d+), (\d+), (\d+)\)')
    # Updated edge pattern: match lines like "Edge((x, y), (x, y), ...)"
    edge_pattern = re.compile(r'^Edge\s*\((.*)\)$')

    for line in lines:
        line = line.strip()
        graph_match = graph_pattern.match(line)
        sep_match = sep_pattern.match(line)
        rank_match = rank_pattern.match(line)
        node_match = node_pattern.match(line)
        quad_match = quad_pattern.match(line)
        edge_match = edge_pattern.match(line)

        if graph_match:
            width, height = map(str_pos_to_coord, graph_match.groups())
            graph_info['width'] = width
            graph_info['height'] = height
        elif sep_match:
            ranksep, nodesep = map(str_pos_to_coord, sep_match.groups())
            graph_info['ranksep'] = ranksep
            graph_info['nodesep'] = nodesep
        elif rank_match:
            rank_id, x, y, width, height = map(str_pos_to_coord, rank_match.groups())
            ranks.append({'id': rank_id, 'x': x, 'y': y, 'width': width, 'height': height})
        elif node_match:
            name = node_match.group(1)
            left, top, right, bottom = map(str_pos_to_coord, node_match.groups()[1:])
            current_node = {'name': name, 'bounds': (left, top, right, bottom), 'quads': []}
            nodes.append(current_node)
        elif quad_match and current_node:
            left, top, right, bottom = map(str_pos_to_coord, quad_match.groups())
            current_node['quads'].append((left, top, right - RIGHT_SUB, bottom - BOTTOM_SUB))
        elif edge_match:
            pts_str = edge_match.group(1)
            # Extract all coordinate pairs
            pts = re.findall(r'\((\d+),\s*(\d+)\)', pts_str)
            pts = [(int(x), int(y)) for (x, y) in pts]
            edges.append(pts)

    return graph_info, ranks, nodes, edges


def main():
    if len(sys.argv) != 2:
        raise RuntimeError("Expected exactly one CLI arg: filepath")
    filepath = sys.argv[1]
    graph_info, ranks, nodes, edges = parse_graph_layout(filepath)

    # Determine bounding box (if needed) and set offsets.
    x_offset = 50
    y_offset = 50

    running = True
    clock = pygame.time.Clock()
    while running:
        screen.fill((255, 255, 255))
        # Draw rank areas
        for rank in ranks:
            pygame.draw.rect(screen, (240, 240, 255),
                             (rank['x'] + x_offset, rank['y'] + y_offset, rank['width'], rank['height']), 0)
            pygame.draw.rect(screen, (200, 200, 230),
                             (rank['x'] + x_offset, rank['y'] + y_offset, rank['width'], rank['height']), 2)

        # Draw nodes
        for node in nodes:
            left, top, right, bottom = node['bounds']
            width = right - left
            height = bottom - top
            pygame.draw.rect(screen, (0, 0, 0), (left + x_offset, top + y_offset, width, height), 2)
            font = pygame.font.SysFont('Arial', 10)
            name_surf = font.render(node['name'], True, (255, 0, 0))
            screen.blit(name_surf, (left + x_offset, top + y_offset - 15))
            for quad in node['quads']:
                q_left, q_top, q_right, q_bottom = quad
                q_width = q_right - q_left
                q_height = q_bottom - q_top
                pygame.draw.rect(screen, (0, 0, 0), (q_left + x_offset, q_top + y_offset, q_width, q_height))

        # Draw edges using the trajectory points.
        for pts in edges:
            if not pts:
                continue
            # Convert each point using ZOOM and offset.
            screen_pts = [(int(x * ZOOM) + x_offset, int(y * ZOOM) + y_offset) for (x, y) in pts]
            pygame.draw.lines(screen, (255, 0, 0), False, screen_pts, 2)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        pygame.display.flip()
        clock.tick(30)

    pygame.quit()


if __name__ == "__main__":
    main()
