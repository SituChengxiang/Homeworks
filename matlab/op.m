function c=op(operation,a,b)
switch operation
case 'addition'
    c=a+b;
case 'subtraction'
    c=a-b;
case 'multiplication'
    c=a*b;
otherwise
    c=a/b;
end