/* Copyright (c) 2024, Meili Authors*/

import functools

# Cluster 
# 8 bf-2 + 4 bf-1


# Return location key
# If there is a stage on the nic, then it is favorable to place other stages(belong to the same pipeline) onto this nic
def location_key (n, A, s):
    for stage in S:
        if stage != s:
            if (not resource_eq(A[stage][n],(0,0,0))):
                return 1
    return 0
                
# Return bandwidth key
# Return the remaining bandwidth of the snic 
def bw_key (W, n, B, A, s):
    B_idle = B[n]                       
    B_sharable = calculate_sharable_bw(W, A, n, s)
    B_allocable = B_sharable + B_idle
    return B_allocable


# Define the key function with multiple arguments
def key_snic (n, B, T, A, G, s , W):
    # Return a tuple of the location, bandwidth, and resource of the snic (core, regex, compression)
    if G[s][0] != 0 and G[s][1] == 0 and G[s][2] == 0:
        return location_key (n, A, s), bw_key (W, n, B, A, s), T[n][0], T[n][1], T[n][2]      
    elif G[s][0] == 0 and G[s][1] != 0 and G[s][2] == 0:    
        return location_key (n, A, s), bw_key (W, n, B, A, s), T[n][1], T[n][0], T[n][2]
    elif G[s][0] == 0 and G[s][1] == 0 and G[s][2] != 0:    
        return location_key (n, A, s), bw_key (W, n, B, A, s), T[n][2], T[n][0], T[n][1]
    else:    
        return location_key (n, A, s), bw_key (W, n, B, A, s), T[n][0], T[n][1], T[n][2]
    # return location_key (n, A, s), bw_key (W, n, B, A, s), T[n][0], T[n][1], T[n][2]

# Calculate the sharable bandwidth provided by parent stage
def calculate_sharable_bw(W, A, n, s):
    if s==0:
        return 0
    return max(A[s-1][n][0] * W[s-1][0], A[s-1][n][1] * W[s-1][1], A[s-1][n][2] * W[s-1][2])


# Input: 
# r - resource vector
# w - per-resource unit bandwidth consumption
# return max(element-wise dot-product) 
def calculate_required_bw(r, w):
    bw = 0
    for i,j in zip(r, w):
        bw = max(bw, i*j)        
    return bw

# Input: two resource vectors
# If element-wise comparison all geq, then return True
def resource_geq(G, T):
    for i,j in zip(G, T):
        if i < j:
            return False    
    return True

# elemen-wise min
def resource_min(r1, r2):
    r3=[]
    for i,j in zip(r1, r2):
        r3.append(min(i,j))  
    return r3  

def resource_eq(G, T):
    for i,j in zip(G, T):
        if i != j:
            return False    
    return True

# Input: two resource vectors
# Element-wise add
def resource_add(r1, r2):
    for i in range(len(r1)):
        r1[i] = r1[i] + r2[i]

# Input: two resource vectors
# Element-wise minus
def resource_minus(r1, r2):
    for i in range(len(r1)):
        r1[i] = r1[i] - r2[i]


# Input: two resource vectors
# Element-wise minus
def resource_multiply(r1, r2):
    r3 = []
    for i,j in zip(r1, r2):
        r3.append(i*j)
    return r3

def resource_divide(num, r1):
    r2 = []
    for i in range(len(r1)):
        if r1[i] != 0:
            r2.append(int(num / r1[i]))
        else:
            r2.append(0)
    return r2

def resource_get_one_hot(r1):
    for i in r1:
        if i != 0:
            return i


def allocate_one_snic(G, T, B, W, A, n, s):
    if ((s != 0) and (calculate_required_bw(G[s], W[s]) > calculate_sharable_bw(W, A, n, s))) or s==0: 

        B_idle = B[n]                       # idle bandwidth
        # if s==0 or A[s-1][n]==0:    
        #     B_sharable = 0 # 0 - sharable bw for stage 0 and stage w/o co-located parent stage
        # elif A[s-1][n]!=0:
        #     B_sharable = calculate_sharable_bw(W, A, n, s) # A[s][n] * W[s] - sharable bw for stage w/ co-located parent stage
        B_sharable = calculate_sharable_bw(W, A, n, s)
        B_allocable = B_sharable + B_idle

        print("B_sharable = " + str(B_sharable))
        print("B_idle = " + str(B_idle))
        print("B_allocable = " + str(B_allocable))
        print("required bw = " + str(calculate_required_bw(G[s], W[s])))
        print("required resource = " + str(G[s]))
        print("available resource = " + str(T[n]))
        if (not resource_geq(T[n], G[s])):
            # required resource is larger than available resource on snic n  
            # allocate at most the amount of available resource on snic n
            if calculate_required_bw(T[n], W[s]) <= B_allocable:
                # When resource is not enough, try allocate as much resource as possible
                
                print("resource not enough, bw enough")
                # Note that since we simplified the input resource vector to be one-hot, here we can directly use minus. If input is not one-hot, it can't
                # A[s][n] = A[s][n] + T[n] 
                # G[s] = G[s] - T[n]
                # T[n] = 0
                r = resource_min(G[s], T[n])
                bw = calculate_required_bw(r, W[s])
                resource_add(A[s][n], r) # allocate resource
                resource_minus(G[s], r) # deduct allocated resource from available resource
                resource_minus(T[n], r) 
                
            else:  
                # bandwidth is not enough 
                print("resource not enough, bw not enough")
                bw = allocate_on_bw(G, T, B_allocable, W, A, n, s)
                
        else:
            # required resource is smaller than available resource on snic n  
            if calculate_required_bw(G[s], W[s]) <= B_allocable:
                print("resource enough, bw enough")
                bw = calculate_required_bw(G[s], W[s])
                resource_add(A[s][n], G[s]) # allocate resource
                resource_minus(T[n], G[s]) # deduct allocated resource from available resource
                resource_minus(G[s], G[s]) # do not required any more resource for the stage, set all elements to zero
                
                print(bw)
            else:
                # bandwidth is not enough 
                print("resource enough, bw not enough")
                bw = allocate_on_bw(G, T, B_allocable, W, A, n, s)  


        B[n] = min(B_idle, B_allocable - bw)
         
    else:
        print("allocation not limited by bw")
        # The allocation has nothing to do with bandwidth since there is already enough bandwidth allocated and stage is not the first one.
        if (not resource_geq(T[n], G[s])):
            # required resource is larger than available resource on snic n  
            print("resource not enough, bw enough")
            r = resource_min(G[s], T[n])
            bw = calculate_required_bw(r, W[s])
            resource_add(A[s][n], r) # allocate resource
            resource_minus(G[s], r) # deduct allocated resource from available resource
            resource_minus(T[n], r) 
            
        else:
            print("resource enough, bw enough")
            
            resource_add(A[s][n], G[s]) # allocate resource
            resource_minus(T[n], G[s]) # deduct allocated resource from available resource
            resource_minus(G[s], G[s]) # do not required any more resource for the stage, set all elements to zero
            

        B[n] = B[n]
    
    return A  


def allocate_on_bw(G, T, B_allocable, W, A, n, s):
    # allocate resource with bandwidth constraint
    # assume input as one-hot
    
    d = resource_divide(B_allocable,W[s])
    resource_add(A[s][n], d) 
    resource_minus(T[n], d) 
    resource_minus(G[s], d)

    bw = resource_multiply(d,W[s])
    bw_value = resource_get_one_hot(bw)
    return bw_value 



# Define the function for resource allocation
def resource_allocation(S, N, G, T, B, W, A):
    # Loop through each stage
    for s in S:
        # Loop until the resource demand is met
        while (not resource_eq(G[s],(0,0,0))):
            # Find the next snic to allocate
            n = find_next_snic(N, T, B, W, A, s)
            # Allocate one snic to the stage
            A = allocate_one_snic(G, T, B, W, A, n, s)
            # debug: print the allocation result after each allocation
            for a in A:
                print(a)
            print(G)
            print(B)
            #x = input()
    # Return the allocation results
    return A

# Define the function for finding the next snic
def find_next_snic(N, T, B, W, A, s):
    # Use the functools.partial function to bind the extra arguments to the key function
    
    key_snic_partial = functools.partial(key_snic, B=B, T=T, A=A, G=G, s=s, W=W)
    # Use the key function with the key parameter
    # Sort the snics by location
    # Sort the snics by bandwidth
    # Sort the snics by resource
    N = sorted (N, key=key_snic_partial, reverse=True)

    for n in N:
        if resource_geq((0,0,0),T[n]):
            print("skipping " + str(n) + " because of no resource " + str(T[n]))
            continue
        elif (not resource_geq((0,0,0),A[s][n])):
            print(s)
            print("skipping " + str(n) + " because of already allocated " + str(A[s][n]))
            continue
        elif G[s][0] != 0 and T[n][0] == 0:
            print("skipping " + str(n) + " because of zero core resource ")
            continue
        elif G[s][1] != 0 and T[n][1] == 0:
            print("skipping " + str(n) + " because of zero regex resource ")
            continue
        elif G[s][2] != 0 and T[n][2] == 0:
            print("skipping " + str(n) + " because of zero compression resource ")
            continue
        elif bw_key(W, n, B, A, s) == 0:
            print("skipping " + str(n) + " because of zero bw resource ")
            continue
        else:
            return n

    return -1


N = [0,1,2,3,4,5,6,7,8,9,10,11] # List of snics
T = [
[15,0,0],
[15,0,0],
[15,0,0],
[15,0,0],
[7,1,1],
[7,1,1],
[7,1,1],
[7,1,1],
[7,1,1],
[7,1,1],
[7,1,1],
[7,1,1]
] # Available resources of snics


# allocation scheme
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
] # Resource allocation results

B = [100,100,100,100,100,100,100,100,100,100,100,100] 
# B = [50,50,50,50,100,100,100,100,100,100,100,100] # Available bandwidth of snics
# B = [100,100,100,100,50,50,50,50,50,50,50,50] # Available bandwidth of snics
# B = [50,50,50,50,50,50,50,50,50,50,50,50] # Available bandwidth of snics
# B = [50,50,50,50,25,25,25,25,25,25,25,25] # Available bandwidth of snics
# B = [25,25,25,25,50,50,50,50,50,50,50,50] # Available bandwidth of snics
# B = [25,25,25,25,25,25,25,25,25,25,25,25] # Available bandwidth of snics
tput_req = 50


# IDS 
print("IDS")
S = [0,1]
G = [[int(tput_req/1.85)+1,0,0],[0,2,0]]
W = [[1.85,0,0],[0,25,0]] 
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
]
resource_allocation(S, N, G, T, B, W, A)
# IPComp GW
print("IPComp GW")
S = [0,1]
G = [[int(tput_req/2.94)+1,0,0],[0,0,2]]
W = [[2.94,0,0],[0,0,25]] 
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
]
resource_allocation(S, N, G, T, B, W, A)
# Firewall
print("Firewall")
S = [0]
G = [[int(tput_req/3.57)+1,0,0]]
W = [[3.57,0,0]] 
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
]
resource_allocation(S, N, G, T, B, W, A)
# Flow Mon
print("Flow Mon")
S = [0]
G = [[int(tput_req/3.125)+1,0,0]]
W = [[3.125,0,0]] 
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
]
resource_allocation(S, N, G, T, B, W, A)
# L7 LB
print("L7 LB")
S = [0]
G = [[int(tput_req/8.33)+1,0,0]]
W = [[8.33,0,0]] 
A = [
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]],
[[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0],[0,0,0]]
]
resource_allocation(S, N, G, T, B, W, A)
  



