% Número de nodos
n = 6;

% Lista de adyacencia (celda donde adjList{i} son los vecinos de i)
adjList = cell(1, n);
adjList{1} = 5; 
adjList{2} = 1;      
adjList{3} = 2;         
adjList{4} = [2 3];      
adjList{5} = [4 6];      
adjList{6} = 4;      

isStronglyConnected = true;

% Para verficar si se puede llegar de un nodo a todos los demás
for i = 1:n
    % Inicializar vector de los nodos que se alcanzan (visitados)
    visited = false(1, n);
    
    % Inicializar stack con nodo i
    stack = i;
    visited(i) = true;

    % DFS (Depth-First Search) iterativo usando stack
    while ~isempty(stack)
        node = stack(end);
        stack(end) = [];  

        neighbors = adjList{node}; % vecinos del nodo actual
        % Si el vecino no ha sido visitado se agrega al stack y se marca
        % como visitado
        for k = 1:length(neighbors)
            neighbor = neighbors(k);
            if ~visited(neighbor)
                stack(end+1) = neighbor; 
                visited(neighbor) = true;
            end
        end
    end

    % Revisar si todos los nodos fueron alcanzados
    if any(~visited)
        isStronglyConnected = false;
        break;
    end
end

% Resultado
if isStronglyConnected
    disp('El grafo es FUERTEMENTE CONEXO.');
else
    disp('El grafo NO es fuertemente conexo.');
end
 